// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The central queues for pipelined data processing
// ****************************************************************************

// Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017,
// 2018, 2019, 2020
// Guy Voncken
//
// This file is part of Guymager.
//
// Guymager is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Guymager is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Guymager. If not, see <http://www.gnu.org/licenses/>.

#include "common.h"
#include "unistd.h"

#include <QSemaphore>
#include <QMutex>
#include <QString>
#include <QStack>

#include "util.h"
#include "config.h"
#include "fifo.h"
#include "stdlib.h"

static QMutex  MutexGlobal;
static quint64 Allocs   =0;
static quint64 Frees    =0;
static qint64  Allocated=0;  // let's use signed, so we can detect underruns


// ------------------------------
//  FIFO block memory management
// ------------------------------

class t_FifoMemory
{
   public:
       t_FifoMemory (unsigned int TotalBlocks, unsigned int AllocBlockSize);
      ~t_FifoMemory ();

       char *GetBlock      (unsigned int Size);
       void  ReleaseBlock  (char *pBlock);
       void  LogBlock      (char *pBlock, char Marker);

   private:
      unsigned int   oTotalBlocks;
      unsigned int   oAllocBlockSize;
      char         *poBuf;
      char         *poBufFirstBlock;
      QStack<char*>  oFreeBlocks;
      QMutex         oMutex;
};

t_FifoMemory::t_FifoMemory (unsigned int TotalBlocks, unsigned int AllocBlockSize)
{
   unsigned int i;
   unsigned int FirstBlockOffset = 0;
   unsigned int BufferOffset     = offsetof (t_FifoBlock, Buffer);
   int          PageSize         = getpagesize();

   oTotalBlocks    = TotalBlocks;
   oAllocBlockSize = AllocBlockSize;

   poBuf = (char *) malloc (TotalBlocks * AllocBlockSize + PageSize);
   if (poBuf == nullptr)
      CHK_EXIT (ERROR_FIFO_MEMORY_ALLOC_FAILED)

   // Calculate offset to have the buffers always lie on a page boundary
   // ------------------------------------------------------------------
   if (((long long)poBuf+BufferOffset) % PageSize)
      FirstBlockOffset = PageSize - ((long long)(poBuf+BufferOffset) % PageSize);
   poBufFirstBlock = poBuf + FirstBlockOffset;

   for (i=0; i<TotalBlocks; i++)
      oFreeBlocks.push (&poBufFirstBlock[i*AllocBlockSize]);
}

t_FifoMemory::~t_FifoMemory (void)
{
   if ((unsigned) oFreeBlocks.count() != oTotalBlocks)
      LOG_ERROR ("Some FIFO blocks have not been released (total: %d, released %d)", oTotalBlocks, oFreeBlocks.count())
   free (poBuf);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
char *t_FifoMemory::GetBlock (unsigned int Size)
{
   char *pBlock = nullptr;

   if (Size > oAllocBlockSize)
   {
      LOG_ERROR ("The requested block size is %u, but only %u sized blocks are available", Size, oAllocBlockSize)
      CHK_EXIT (ERROR_FIFO_MEMORY_INVALID_BLOCKSIZE)
   }
   oMutex.lock();
      if (!oFreeBlocks.isEmpty())
         pBlock = oFreeBlocks.pop();  // Warning strict-overflow occurs because of this line... reason never was found and warning thus has been switched off for this function.
   oMutex.unlock();

   return pBlock;
}
#pragma GCC diagnostic pop


void t_FifoMemory::ReleaseBlock (char *pBlock)
{
   unsigned int Ofs, i;

   // Safety first
   // ------------
   if (pBlock < poBufFirstBlock)
      CHK_EXIT (ERROR_FIFO_POINTER_TOO_LOW)

   Ofs = pBlock - poBufFirstBlock;
   if ((Ofs % oAllocBlockSize) != 0)
      CHK_EXIT (ERROR_FIFO_POINTER_NOT_ON_BOUNDARY)

   i = Ofs / oAllocBlockSize;
   if (i >= oTotalBlocks)
      CHK_EXIT (ERROR_FIFO_POINTER_TOO_HIGH)

   // Give back the pointer
   // ---------------------
   oMutex.lock();
      oFreeBlocks.push (pBlock);
   oMutex.unlock();
}


void t_FifoMemory::LogBlock (char *pBlock, char Marker) // For debugging purposes
{
   int i;

   i = (pBlock - poBuf) / oAllocBlockSize;
   oMutex.lock();
   printf ("%c%d ", Marker, i);
   oMutex.unlock();
}


APIRET FifoMemoryAlloc (t_pFifoMemory *ppFifoMemory, unsigned int TotalBlocks, unsigned int AllocBlockSize)
{
   if (*ppFifoMemory)
      CHK_EXIT (ERROR_FIFO_MEMORY_ALREADY_ALLOCATED)
   *ppFifoMemory = new t_FifoMemory (TotalBlocks, AllocBlockSize);

   return NO_ERROR;
}

APIRET FifoMemoryFree (t_pFifoMemory *ppFifoMemory)
{
   if (*ppFifoMemory == nullptr)
      CHK_EXIT (ERROR_FIFO_MEMORY_NOT_ALLOCATED)
   delete *ppFifoMemory;
   *ppFifoMemory = nullptr;

   return NO_ERROR;
}

// ----------------------------
//     FIFO block handling
// ----------------------------

APIRET t_Fifo::Create (t_pFifoMemory pFifoMemory, t_pFifoBlock &pBlock, unsigned int AllocSize)
{
   if (CONFIG(FifoMemoryManager))
        pBlock = (t_pFifoBlock) pFifoMemory->GetBlock (AllocSize);
   else pBlock = (t_pFifoBlock) UTIL_MEM_ALLOC        (AllocSize);

   if (pBlock == nullptr)
      CHK_CONST (ERROR_FIFO_MALLOC_FAILED)
//   memset (pBlock, 0xAA, sizeof(t_FifoBlock) + BufferSize);

   memset (pBlock, 0, sizeof(t_FifoBlock));
   pBlock->MagicStart         = FIFO_MAGIC_START;
   pBlock->BufferSize         = AllocSize - sizeof (t_FifoBlock);
//   pBlock->DataSize           = 0;
//   pBlock->LastBlock          = false;

//   pBlock->EwfPreprocessed    = false;
//   pBlock->EwfDataSize        = 0;
//   pBlock->EwfCompressionUsed = 0;
//   pBlock->EwfChunkCRC        = 0;
//   pBlock->EwfWriteCRC        = 0;

//   pBlock->pAaffPreprocess    = nullptr;
//   pBlock->pAewfPreprocess    = nullptr;

   pBlock->Nr                 = FIFO_NR_NOTSET;

   //lint -save -e826 suspicious ptr to ptr conversion
   FIFO_SET_MAGIC_END (pBlock)
   //lint -restore

   MutexGlobal.lock();
   Allocs++;
   Allocated += AllocSize;
   MutexGlobal.unlock();

   return NO_ERROR;
}

APIRET t_Fifo::Duplicate(t_pFifoMemory pFifoMemory, t_pFifoBlock pBlockSrc, t_pFifoBlock &pBlockDst)
{
   pBlockDst = nullptr;
   if (pBlockSrc == nullptr)  // Dummy block
      return NO_ERROR;

   unsigned int AllocSize = pBlockSrc->BufferSize + sizeof(t_FifoBlock);

   if (CONFIG(FifoMemoryManager))
        pBlockDst = (t_pFifoBlock) pFifoMemory->GetBlock (AllocSize);
   else pBlockDst = (t_pFifoBlock) UTIL_MEM_ALLOC        (AllocSize);

   if (pBlockDst == nullptr)
      CHK_CONST (ERROR_FIFO_MALLOC_FAILED)

   memcpy (pBlockDst, pBlockSrc, AllocSize);

   MutexGlobal.lock();
   Allocs++;
   Allocated += AllocSize;
   MutexGlobal.unlock();

   return NO_ERROR;
}

//lint -save -e661 -e826 -esym(613,pBlock) Possible access of out-of-bounds pointer, suspicious ptr to ptr conversion, possible use of nullptr
APIRET t_Fifo::Destroy (t_pFifoMemory pFifoMemory, t_pFifoBlock &pBlock)
{
   int TotalSize;

   if (pBlock == nullptr)
      CHK (ERROR_FIFO_DOUBLE_FREE)

   if (pBlock->MagicStart != FIFO_MAGIC_START)
   {
      LOG_ERROR ("Broken start magic: %08X", pBlock->MagicStart);
      CHK (ERROR_FIFO_START_CORRUPTED)
   }

   if (FIFO_GET_MAGIC_END(pBlock) != FIFO_MAGIC_END)
   {
      LOG_ERROR ("Broken end magic: %08X", FIFO_GET_MAGIC_END(pBlock));
      CHK (ERROR_FIFO_END_CORRUPTED)
   }
   TotalSize = pBlock->BufferSize + sizeof(t_FifoBlock);

//   memset (pBlock, 0xBB, TotalSize);

   if (CONFIG(FifoMemoryManager))
        pFifoMemory->ReleaseBlock ((char *)pBlock);
   else UTIL_MEM_FREE             (        pBlock);
   pBlock = nullptr;

   MutexGlobal.lock();
   Frees++;
   Allocated -= TotalSize;
   if (Allocated < 0)
   {
      LOG_ERROR ("Memory underrun");
      MutexGlobal.unlock();
      CHK (ERROR_FIFO_END_CORRUPTED)
   }
   MutexGlobal.unlock();

   return NO_ERROR;
}
//lint -restore

void t_Fifo::LogBlock (t_pFifoMemory pFifoMemory, t_pFifoBlock pBlock, char Marker)
{
   pFifoMemory->LogBlock ((char *)pBlock, Marker);
}


// ----------------------------
//        Standard FIFO
// ----------------------------

class t_FifoStdLocal
{
   public:
      QQueue<t_pFifoBlock>  Queue;
      int                   MaxBlocks;
      QSemaphore          *pSemEmptyBlocks;
      QSemaphore          *pSemUsedBlocks;
      QMutex              *pMutexQueue;
      t_pFifoMemory        pFifoMemory;
      t_pFifoStd           pFifoDup;
};

t_FifoStd::t_FifoStd (t_pFifoMemory pFifoMemory, int MaxBlocks)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_MALLOC_FAILED            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_DOUBLE_FREE              ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_EMPTY                    ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_END_CORRUPTED            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_START_CORRUPTED          ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_MEMORY_UNDERUN           ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_MEMORY_ALLOC_FAILED      ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_MEMORY_INVALID_BLOCKSIZE ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_MEMORY_ALREADY_ALLOCATED ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_MEMORY_NOT_ALLOCATED     ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_POINTER_TOO_LOW          ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_POINTER_NOT_ON_BOUNDARY  ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_POINTER_TOO_HIGH         ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_ALREADY_DUPLICATED       ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FIFO_DUPLICATION_NOT_SUPPORTED))
   }
   pOwn = new t_FifoStdLocal;
   pOwn->pFifoMemory = pFifoMemory;
   pOwn->MaxBlocks   = MaxBlocks;
   pOwn->pFifoDup    = nullptr;

   pOwn->pSemEmptyBlocks = new QSemaphore (MaxBlocks);
   pOwn->pSemUsedBlocks  = new QSemaphore (0);
   pOwn->pMutexQueue     = new QMutex     ();
}

t_FifoStd::~t_FifoStd ()
{
   t_pFifoBlock pBlock;

   while (!pOwn->Queue.isEmpty())
   {
      CHK_EXIT (t_FifoStd::Get (pBlock))
      if (pBlock)
         CHK_EXIT (Destroy (pOwn->pFifoMemory, pBlock))
   }

   if (pOwn->pFifoDup)
      delete pOwn->pFifoDup;
   delete pOwn->pSemEmptyBlocks;
   delete pOwn->pSemUsedBlocks;
   delete pOwn->pMutexQueue;
   delete pOwn;
}

APIRET t_FifoStd::DupFifo (t_pFifo *ppFifo)
{
   *ppFifo = nullptr;

   if (pOwn->pFifoDup)
      return ERROR_FIFO_ALREADY_DUPLICATED;

   pOwn->pFifoDup = new t_FifoStd (pOwn->pFifoMemory, pOwn->MaxBlocks);
   *ppFifo = pOwn->pFifoDup;

   return NO_ERROR;
}

APIRET t_FifoStd::Insert (t_pFifoBlock pBlock)
{
   if (pOwn->pFifoDup)                   // Important: Duplicate before inserting into this fifo, or else the block
   {                                     // could already have been taken out of the Fifo and destroyed.
      t_pFifoBlock pBlockDup;
      CHK (Duplicate (pOwn->pFifoMemory, pBlock, pBlockDup));
      CHK (pOwn->pFifoDup->Insert(pBlockDup))
   }

   pOwn->pSemEmptyBlocks->acquire();
   pOwn->pMutexQueue->lock();
   pOwn->Queue.enqueue (pBlock);
   pOwn->pMutexQueue->unlock();
   pOwn->pSemUsedBlocks->release();

   return NO_ERROR;
}

APIRET t_FifoStd::InsertDummy (void)
{
   CHK (Insert (nullptr));
   return NO_ERROR;
}

APIRET t_FifoStd::Get (t_pFifoBlock &pBlock)
{
   pOwn->pSemUsedBlocks->acquire();
   if (pOwn->Queue.isEmpty())
      CHK (ERROR_FIFO_EMPTY)
   pOwn->pMutexQueue->lock();
   pBlock = pOwn->Queue.dequeue();
   pOwn->pMutexQueue->unlock();
   pOwn->pSemEmptyBlocks->release();

   return NO_ERROR;
}

APIRET t_FifoStd::Count (int &Cnt)
{
   pOwn->pMutexQueue->lock();
   Cnt = pOwn->Queue.count();
   pOwn->pMutexQueue->unlock();
   return NO_ERROR;
}

APIRET t_FifoStd::Usage (int &Percent)
{
   int Cnt;
   int Max = pOwn->MaxBlocks;

   CHK (Count(Cnt))
   if (pOwn->pFifoDup)
   {
      int CntDup;
      CHK (pOwn->pFifoDup->Count(CntDup))
      Cnt += CntDup;
      Max *= 2;
   }
   Percent = (100LL * Cnt) / Max;
   return NO_ERROR;
}


// Qt provides no possibility for waking threads waiting for semaphores. This fn wakes a potential
// thread hanging in t_FifoStd::Get by inserting a nullptr element into the Fifo. Threads hanging in
// t_FifoStd::Insert are woken by removing a block from the Fifo. Of course, this method is not really
// clean, as it not only wakes threads but manipulates the data in the Fifo... the threads accessing
// the Fifo must be able to cope with this.
// By the way: A non-blocking semaphore could be implemented manually by putting QSemaphore::tryAcquire
// together with a msleep(0) in to loop. But this solution turned out to be non-optimal in performance.

APIRET t_FifoStd::WakeWaitingThreads (void)
{
   t_pFifoBlock pBlock;
   bool          WouldBlock;

   WouldBlock = !pOwn->pSemUsedBlocks->tryAcquire();
   if (WouldBlock)                         // If this access would block there might be a thread waiting in "Get"
        CHK (Insert (nullptr))
   else pOwn->pSemUsedBlocks->release();

   WouldBlock = !pOwn->pSemEmptyBlocks->tryAcquire();
   if (WouldBlock)                         // If this access would block there might be a thread waiting in "Insert"
   {
      CHK (Get (pBlock))
      if (pBlock)
         CHK (t_Fifo::Destroy (pOwn->pFifoMemory, pBlock))
   }
   else
   {
      pOwn->pSemEmptyBlocks->release();
   }

   if (pOwn->pFifoDup)
      CHK (pOwn->pFifoDup->WakeWaitingThreads())

   return NO_ERROR;
}



// ----------------------------------------------------------------------------------------
//                                       FIFO Compress In
// ----------------------------------------------------------------------------------------

class t_FifoCompressLocal   // used as well for t_FifoCompressOut
{
   public:
      t_pFifoStd *ppFifoArr;
      int           FifoArrLen;
      int           TotalMaxBlocks;    // Sum of MaxBlocks of all FIFOs
      QMutex      *pMutexQueue;
      int           NextSubFifoNr;
      t_pFifo      pFifoDup;
      bool          Dup;               // Set if this is a duplicated FIFO (i.e. which refers to the duplicated
                                       // subfifos of another t_FifoCompressOut in ints ppFifoArr).
};

static APIRET FifoCompressInit (t_FifoCompressLocal **ppOwn, t_pFifoMemory pFifoMemory, int SubFifos, int MaxBlocks)
{
   *ppOwn = new t_FifoCompressLocal;
   (*ppOwn)->TotalMaxBlocks = SubFifos * MaxBlocks;
   (*ppOwn)->FifoArrLen     = SubFifos;
   (*ppOwn)->ppFifoArr      = (t_pFifoStd *) UTIL_MEM_ALLOC ((size_t)SubFifos * sizeof(t_pFifoStd));

   for (int i=0; i<SubFifos; i++)
      (*ppOwn)->ppFifoArr[i] = new t_FifoStd (pFifoMemory, MaxBlocks);

   (*ppOwn)->NextSubFifoNr = 0;
   (*ppOwn)->pMutexQueue   = new QMutex();
   (*ppOwn)->pFifoDup      = nullptr;
   (*ppOwn)->Dup           = false;

   return NO_ERROR;
}

static APIRET FifoCompressDeInit (t_FifoCompressLocal *pOwn, bool RemoveSubFifos=true)
{
   if (RemoveSubFifos)
   {
      for (int i=0; i<pOwn->FifoArrLen; i++)
         delete (pOwn->ppFifoArr[i]);
   }
   UTIL_MEM_FREE (pOwn->ppFifoArr);
   delete pOwn->pMutexQueue;
   delete pOwn;

   return NO_ERROR;
}

t_FifoCompressIn::t_FifoCompressIn (void)
{
   pOwn=nullptr;   //lint -esym(613,t_FifoCompressIn::pOwn)  Prevent lint from telling us about possible null pointers in the following code
   CHK_EXIT (ERROR_FIFO_CONSTRUCTOR_NOT_SUPPORTED)
}

t_FifoCompressIn::t_FifoCompressIn (t_pFifoMemory pFifoMemory, int SubFifos, int MaxBlocks)
{
   CHK_EXIT (FifoCompressInit (&pOwn, pFifoMemory, SubFifos, MaxBlocks))
}

t_FifoCompressIn::~t_FifoCompressIn ()
{
   CHK_EXIT (FifoCompressDeInit (pOwn))
}

APIRET t_FifoCompressIn::DupFifo (t_pFifo */*ppFifo*/)
{
   return ERROR_FIFO_DUPLICATION_NOT_SUPPORTED;
}

APIRET t_FifoCompressIn::GetSubFifo (int SubFifoNr, t_pFifoStd &pFifo)
{
   pFifo = pOwn->ppFifoArr[SubFifoNr];

   return NO_ERROR;
}

APIRET t_FifoCompressIn::Insert (t_pFifoBlock pBlock)
{
   t_pFifoStd pFifo;

   pOwn->pMutexQueue->lock();
      CHK (GetSubFifo (pOwn->NextSubFifoNr++, pFifo))
      if (pOwn->NextSubFifoNr == pOwn->FifoArrLen)
         pOwn->NextSubFifoNr = 0;
   pOwn->pMutexQueue->unlock();

   CHK (pFifo->Insert (pBlock))

   return NO_ERROR;
}

APIRET t_FifoCompressIn::InsertDummy (void)
{
   for (int i=0; i< pOwn->FifoArrLen; i++)
      CHK (Insert (nullptr))

   return NO_ERROR;
}

APIRET t_FifoCompressIn::Get (t_pFifoBlock &/*pBlock*/)
{
   return ERROR_FIFO_FUNCTION_NOT_IMPLEMENTED;
}

APIRET t_FifoCompressIn::Count (int &Cnt)
{
   t_pFifoStd pFifo;
   int         SubCnt;

   Cnt = 0;
//   pOwn->pMutexQueue->lock();                           // Do not use semaphores here because of the deadlock risk.
      for (int i=0; i<pOwn->FifoArrLen; i++)              // As a consequence, the count might be slightly wrong if
      {                                                   // other threads are inserting and deleting in parallel,
         CHK (GetSubFifo (i, pFifo))                      // but that does no harm.
         CHK (pFifo->Count (SubCnt))
         Cnt += SubCnt;
      }
//   pOwn->pMutexQueue->unlock();

   return NO_ERROR;
}

APIRET t_FifoCompressIn::Usage (int &Percent)
{
   int Cnt;

   CHK (Count(Cnt))
   Percent = (100LL * Cnt) / pOwn->TotalMaxBlocks;
   Percent = GETMIN (Percent, 100); // In some cases, the usage could be above 100% (see remarks for t_FifoCompressIn::Count); so we limit to 100
   return NO_ERROR;
}

APIRET t_FifoCompressIn::WakeWaitingThreads (void)
{
   t_pFifoStd pFifo;

   for (int i=0; i< pOwn->FifoArrLen; i++)
   {
      CHK (GetSubFifo (i, pFifo))
      CHK (pFifo->WakeWaitingThreads())
   }

   return NO_ERROR;
}


// ----------------------------------------------------------------------------------------
//                                       FIFO Compress Out
// ----------------------------------------------------------------------------------------

t_FifoCompressOut::t_FifoCompressOut ()
{
   pOwn = nullptr;   //lint -esym(613,t_FifoCompressOut::pOwn)  Prevent lint from telling us about possible null pointers in the following code
   CHK_EXIT (ERROR_FIFO_CONSTRUCTOR_NOT_SUPPORTED)
}

t_FifoCompressOut::t_FifoCompressOut (t_pFifoCompressOut pFifoSrc)
{
   pOwn = new t_FifoCompressLocal;
   pOwn->TotalMaxBlocks = pFifoSrc->pOwn->TotalMaxBlocks;
   pOwn->FifoArrLen     = pFifoSrc->pOwn->FifoArrLen;
   pOwn->ppFifoArr      = (t_pFifoStd *) UTIL_MEM_ALLOC ((size_t)pOwn->FifoArrLen * sizeof(t_pFifoStd));

   for (int i=0; i<pOwn->FifoArrLen; i++)
      pOwn->ppFifoArr[i] = nullptr;

   pOwn->NextSubFifoNr = 0;
   pOwn->pMutexQueue   = new QMutex();
   pOwn->pFifoDup      = nullptr;
   pOwn->Dup           = true;
}

t_FifoCompressOut::t_FifoCompressOut (t_pFifoMemory pFifoMemory, int SubFifos, int MaxBlocks)
{
   CHK_EXIT (FifoCompressInit (&pOwn, pFifoMemory, SubFifos, MaxBlocks))
}

t_FifoCompressOut::~t_FifoCompressOut ()
{
   if (pOwn->pFifoDup)
      delete (pOwn->pFifoDup);
   CHK_EXIT (FifoCompressDeInit (pOwn, !pOwn->Dup))
}

APIRET t_FifoCompressOut::DupFifo (t_pFifo *ppFifo)
{
   t_pFifoCompressOut   pFifoCompressOutDup;
   t_pFifo              pSubFifoDup;
   t_FifoCompressLocal *pDupOwn;

   pFifoCompressOutDup = new t_FifoCompressOut (this);
   pDupOwn = pFifoCompressOutDup->pOwn;
   for (int i=0; i<pOwn->FifoArrLen; i++)
   {
      pOwn->ppFifoArr[i]->DupFifo (&pSubFifoDup);
      pDupOwn->ppFifoArr[i] = (t_pFifoStd) pSubFifoDup;
   }

   *ppFifo = pFifoCompressOutDup;

   return NO_ERROR;
}

APIRET t_FifoCompressOut::GetSubFifo (int SubFifoNr, t_pFifoStd &pFifo)
{
   pFifo = pOwn->ppFifoArr[SubFifoNr];

   return NO_ERROR;
}

APIRET t_FifoCompressOut::Insert (t_pFifoBlock /*pBlock*/)
{
   return ERROR_FIFO_FUNCTION_NOT_IMPLEMENTED;
}

APIRET t_FifoCompressOut::InsertDummy (void)
{
   return ERROR_FIFO_FUNCTION_NOT_IMPLEMENTED;
}

APIRET t_FifoCompressOut::InsertDummy (int SubFifoNr)
{
   t_pFifoStd pFifo;

   CHK (GetSubFifo (SubFifoNr, pFifo))
   CHK (pFifo->Insert (nullptr))

   return NO_ERROR;
}

APIRET t_FifoCompressOut::Get (t_pFifoBlock &pBlock)
{
   t_pFifoStd pFifo;

   pOwn->pMutexQueue->lock();
      CHK (GetSubFifo (pOwn->NextSubFifoNr++, pFifo))
      if (pOwn->NextSubFifoNr == pOwn->FifoArrLen)
         pOwn->NextSubFifoNr = 0;
   pOwn->pMutexQueue->unlock();

   CHK (pFifo->Get (pBlock));

   return NO_ERROR;
}

APIRET t_FifoCompressOut::Count (int &Cnt)
{
   t_pFifoStd pFifo;
   int         SubCnt;

   Cnt = 0;
//   pOwn->pMutexQueue->lock();
      for (int i=0; i<pOwn->FifoArrLen; i++)
      {
         CHK (GetSubFifo (i, pFifo))
         CHK (pFifo->Count (SubCnt))
         Cnt += SubCnt;
      }
//   pOwn->pMutexQueue->unlock();

   return NO_ERROR;
}

APIRET t_FifoCompressOut::Usage (int &Percent)
{
   int Cnt;
   int Max = pOwn->TotalMaxBlocks;

   CHK (Count(Cnt))
   if (pOwn->pFifoDup)
   {
      int CntDup;
      CHK (pOwn->pFifoDup->Count(CntDup))
      Cnt += CntDup;
      Max *= 2;
   }
   Percent = (100LL * Cnt) / Max;
   Percent = GETMIN (Percent, 100); // In some cases, the usage could be above 100% (see remarks for t_FifoCompressIn::Count); so we limit to 100
   return NO_ERROR;
}

APIRET t_FifoCompressOut::WakeWaitingThreads (int SubFifoNr)
{
   t_pFifoStd pFifo;

   CHK (GetSubFifo (SubFifoNr, pFifo))
   CHK (pFifo->WakeWaitingThreads())

   return NO_ERROR;
}

APIRET t_FifoCompressOut::WakeWaitingThreads (void)
{
   for (int i=0; i<pOwn->FifoArrLen; i++)
      CHK (WakeWaitingThreads (i))

   return NO_ERROR;
}

APIRET FifoGetStatistics (quint64 &AllocCalls, quint64 &FreeCalls, qint64 &AllocatedMemory)
{
   MutexGlobal.lock();
   AllocCalls      = Allocs;
   FreeCalls       = Frees;
   AllocatedMemory = Allocated;
   MutexGlobal.unlock();

   return NO_ERROR;
}


