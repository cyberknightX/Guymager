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

#ifndef FIFO_H
#define FIFO_H

#include <QQueue>

#ifndef COMMON_H
   #include "common.h"
#endif


#include "ewf.h"

#include "aaff.h"
#include "aewf.h"



class   t_FifoMemory;
typedef t_FifoMemory *t_pFifoMemory;

APIRET FifoMemoryAlloc   (t_pFifoMemory *ppFifoMemory, unsigned int TotalBlocks, unsigned int BlockSize);
APIRET FifoMemoryFree    (t_pFifoMemory *ppFifoMemory);
APIRET FifoGetStatistics (quint64 &AllocCalls, quint64 &FreeCalls, qint64 &AllocatedMemory);



class t_FifoStdLocal;
class t_FifoStdDupLocal;
class t_FifoCompressLocal;

const int     FIFO_MAGIC_START = (int)0x0BADCAFE;
const int     FIFO_MAGIC_END   = (int)0xDEADBEEF;
const quint64 FIFO_NR_NOTSET   = Q_UINT64_C (0xFFFFFFFFFFFFFFFF);

typedef struct
{
   int                 MagicStart;
   unsigned int        BufferSize;            // The size of the buffer, might be something bigger than needed in order to optimise memory management (see memo.txt)
   unsigned int        DataSize;              // The size of the original, uncompressed data (the actual data in the buffer might be compressed)
   bool                LastBlock;             // Only set for the last data block - this flag is used in AFF compression, where the last block must be treated differently
   bool                EwfPreprocessed;       // This block contains data preprocessed for libewf and the following EwfXxx vars contain valid data for ewflib
   size_t              EwfDataSize;           // For libewf
   int8_t              EwfCompressionUsed;    // For libewf
   uint32_t            EwfChunkCRC;           // For libewf
   int8_t              EwfWriteCRC;           // For libewf

   t_AaffPreprocess    AaffPreprocess;
   t_AewfPreprocess    AewfPreprocess;

   qulonglong          Nr;                    // Set to FIFO_NR_NOTSET if nr is not known yet
   unsigned char       Buffer[0];             //lint !e1501  Has zero size
   int                 MagicEnd;              // Do not reference MagicEnd in this structure, use the macros below instead! MagicEnd 
                                              // nevertheless must not be commented out as sizeof calculation would otherwise become wrong
} t_FifoBlock, *t_pFifoBlock;

#define FIFO_SET_MAGIC_END(pFifoBlock)  *((int *)&(pFifoBlock->Buffer[pFifoBlock->BufferSize])) = FIFO_MAGIC_END;
#define FIFO_GET_MAGIC_END(pFifoBlock) (*((int *)&(pFifoBlock->Buffer[pFifoBlock->BufferSize])))

// -------------------------
//  Virtual FIFO base class
// -------------------------

class   t_Fifo;
typedef t_Fifo *t_pFifo;
class   t_Fifo
{
   public:
      virtual ~t_Fifo  (void) {};

      static  APIRET Create   (t_pFifoMemory pFifoMemory, t_pFifoBlock &pBlock, unsigned int AllocSize);
      static  APIRET Duplicate(t_pFifoMemory pFifoMemory, t_pFifoBlock  pBlockSrc, t_pFifoBlock &pBlockDst);
      static  APIRET Destroy  (t_pFifoMemory pFifoMemory, t_pFifoBlock &pBlock);
      static  void   LogBlock (t_pFifoMemory pFifoMemory, t_pFifoBlock  pBlock, char Marker);

      static  unsigned int GetBufferOverhead      (void);
      static  unsigned int GetOptimisedBufferSize (int BlockSize);

      virtual APIRET DupFifo            (t_pFifo *ppFifo)      = 0;
      virtual APIRET Insert             (t_pFifoBlock  pBlock) = 0;
      virtual APIRET InsertDummy        (void)                 = 0;
      virtual APIRET Get                (t_pFifoBlock &pBlock) = 0;
      virtual APIRET Count              (int &Count)           = 0;
      virtual APIRET Usage              (int &Usage)           = 0;
      virtual APIRET WakeWaitingThreads (void)                 = 0;
};

typedef t_Fifo *t_pFifo;


// ---------------
//  Standard FIFO
// ---------------

class   t_FifoStd;
typedef t_FifoStd *t_pFifoStd;
class   t_FifoStd: public t_Fifo
{
   public:
      t_FifoStd (t_pFifoMemory pFifoMemory, int MaxBlocks=128);
     ~t_FifoStd ();

      APIRET DupFifo            (t_pFifo *ppFifo);
      APIRET Insert             (t_pFifoBlock  pBlock);
      APIRET InsertDummy        (void);
      APIRET Get                (t_pFifoBlock &pBlock);
      APIRET Count              (int &Count);
      APIRET Usage              (int &Percent);
      APIRET WakeWaitingThreads (void);

   private:
      t_FifoStdLocal *pOwn;
};


// -------------------
//  Compression FIFOs
// -------------------

class   t_FifoCompressIn;
class   t_FifoCompressOut;
typedef t_FifoCompressIn  *t_pFifoCompressIn;
typedef t_FifoCompressOut *t_pFifoCompressOut;

class t_FifoCompressIn: public t_Fifo
{
   public:
      t_FifoCompressIn (void);
      t_FifoCompressIn (t_pFifoMemory pFifoMemory, int SubFifos, int MaxBlocks=128);
     ~t_FifoCompressIn ();

      APIRET DupFifo            (t_pFifo *ppFifo);
      APIRET Insert             (t_pFifoBlock  pBlock);
      APIRET InsertDummy        (void);
      APIRET Get                (t_pFifoBlock &pBlock);
      APIRET Count              (int &Count);
      APIRET Usage              (int &Percent);
      APIRET WakeWaitingThreads (void);

      APIRET GetSubFifo         (int SubFifoNr, t_pFifoStd &pFifo);

   private:
      t_FifoCompressLocal *pOwn;
};

class t_FifoCompressOut: public t_Fifo
{
   private:
      t_FifoCompressOut (t_pFifoCompressOut pFifoSrc);

   public:
      t_FifoCompressOut ();
      t_FifoCompressOut (t_pFifoMemory pFifoMemory, int SubFifos, int MaxBlocks=128);
      virtual ~t_FifoCompressOut ();

      APIRET DupFifo            (t_pFifo *ppFifo);
      APIRET Insert             (t_pFifoBlock  pBlock);
      APIRET InsertDummy        (void);
      APIRET Get                (t_pFifoBlock &pBlock);
      APIRET Count              (int &Count);
      APIRET Usage              (int &Percent);
      APIRET WakeWaitingThreads (void);

      APIRET GetSubFifo         (int SubFifoNr, t_pFifoStd &pFifo);
      APIRET WakeWaitingThreads (int SubFifoNr);
      APIRET InsertDummy        (int SubFifoNr);

   private:
      t_FifoCompressLocal *pOwn;
};


// ------------------------------------
//             Error codes
// ------------------------------------

   #ifdef MODULES_H
      enum
      {
         ERROR_FIFO_MALLOC_FAILED = ERROR_BASE_FIFO + 1,
         ERROR_FIFO_DOUBLE_FREE,
         ERROR_FIFO_EMPTY,
         ERROR_FIFO_END_CORRUPTED,
         ERROR_FIFO_START_CORRUPTED,
         ERROR_FIFO_MEMORY_UNDERUN,
         ERROR_FIFO_FUNCTION_NOT_IMPLEMENTED,
         ERROR_FIFO_CONSTRUCTOR_NOT_SUPPORTED,
         ERROR_FIFO_MEMORY_ALLOC_FAILED,
         ERROR_FIFO_MEMORY_INVALID_BLOCKSIZE,
         ERROR_FIFO_MEMORY_ALREADY_ALLOCATED,
         ERROR_FIFO_MEMORY_NOT_ALLOCATED,
         ERROR_FIFO_POINTER_TOO_LOW,
         ERROR_FIFO_POINTER_NOT_ON_BOUNDARY,
         ERROR_FIFO_POINTER_TOO_HIGH,
         ERROR_FIFO_ALREADY_DUPLICATED,
         ERROR_FIFO_DUPLICATION_NOT_SUPPORTED
      };
   #endif

#endif

