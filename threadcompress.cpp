// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Multi-threaded compression
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

#include <QtCore>

#include "common.h"
#include "device.h"
#include "threadcompress.h"
#include "threadwrite.h"
#include "config.h"
#include "aaff.h"
#include "util.h"

class t_ThreadCompressLocal
{
   public:
      t_pDevice pDevice;
      int        ThreadNr;
};

t_ThreadCompress::t_ThreadCompress (void)
{
   CHK_EXIT (ERROR_THREADCOMPRESS_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 not initialised


t_ThreadCompress::t_ThreadCompress (t_pDevice pDevice, int ThreadNr)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADCOMPRESS_CONSTRUCTOR_NOT_SUPPORTED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADCOMPRESS_ZLIB_FAILED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADCOMPRESS_LIBEWF_FAILED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADCOMPRESS_COMPRESSION_BUFFER_TOO_SMALL))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADCOMPRESS_INVALID_FORMAT))

      Initialised = true;
   }

   pOwn = new t_ThreadCompressLocal;
   pOwn->pDevice  = pDevice;
   pOwn->ThreadNr = ThreadNr;

   CHK_QT_EXIT (connect (this, SIGNAL(finished()), this, SLOT(SlotFinished())))
}

t_ThreadCompress::~t_ThreadCompress (void)
{
   delete pOwn;
}

APIRET t_ThreadCompress::GetFifoExtraSpace (t_pDevice pDevice, unsigned int *pExtraSpace)
{
   unsigned int ExtraSpace;
   unsigned int ExtraZ;

   ExtraZ = UtilGetMaxZcompressedBufferSize (pDevice->FifoBlockSize) - pDevice->FifoBlockSize;

   switch (pDevice->Acquisition1.Format)
   {
      case t_File::DD  : ExtraSpace = 0;   break;
      case t_File::AAFF: ExtraSpace = AaffPreprocessExtraSpace(pDevice->FifoBlockSize);  break;
      case t_File::EWF:
         if (CONFIG(EwfFormat) == t_File::AEWF)
              ExtraSpace = AewfPreprocessExtraSpace(pDevice->FifoBlockSize);
         else ExtraSpace = ExtraZ;
         break;
      default:  CHK (ERROR_THREADCOMPRESS_INVALID_FORMAT)
   }
   *pExtraSpace = ExtraSpace;
   return NO_ERROR;
}


void t_ThreadCompress::run (void)
{
   t_pDevice      pDevice;
   t_pFifoStd     pFifoIn;
   t_pFifoStd     pFifoOut;
   t_pFifoBlock   pFifoBlockIn;
   t_pFifoBlock   pFifoBlockOut;
   bool            Finished  = false;
   quint64         Blocks    = 0;
   const int       SubFifoNr = pOwn->ThreadNr;
   #if (ENABLE_LIBEWF)
      size_t          CompressedMaxSize = 0;
      size_t          CompressedSize    = 0;
      ssize_t         rc;
      LIBEWF_HANDLE *pEwfHandle  = nullptr;
   #endif

   LOG_INFO ("[%s] Compression thread #%d started", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice), pOwn->ThreadNr)
   CHK_EXIT (SetDebugMessage ("Start run function"))

   pDevice  = pOwn->pDevice;

   #if (ENABLE_LIBEWF)
      if (pDevice->Acquisition1.Format == t_File::EWF)
      {
         CompressedMaxSize = (size_t) (pDevice->FifoBlockSize * 1.001) + 12; // see zlib documentation
         if (CONFIG(EwfFormat) != t_File::AEWF)
         {
            void *pHandle;
            CHK_EXIT (pDevice->pThreadWrite1->GetpFileHandle (&pHandle))  // This handle is the reason why libewf can't
            pEwfHandle  = (LIBEWF_HANDLE *) pHandle;                      // be used together with image duplication
         }
      }
   #endif

   CHK_EXIT (pDevice->pFifoCompressIn ->GetSubFifo (SubFifoNr, pFifoIn ))
   CHK_EXIT (pDevice->pFifoCompressOut->GetSubFifo (SubFifoNr, pFifoOut))

   while (!Finished && !pDevice->Error.Abort())
   {
      CHK_EXIT (SetDebugMessage ("Wait for block"))
      CHK_EXIT (pFifoIn->Get (pFifoBlockIn))
      CHK_EXIT (SetDebugMessage ("Process block"))
      if (pFifoBlockIn)
      {
         Blocks++;
         CHK_EXIT (t_Fifo::Create (pDevice->pFifoMemory, pFifoBlockOut, pDevice->FifoAllocBlockSize))
         pFifoBlockOut->Nr       = pFifoBlockIn->Nr;
         pFifoBlockOut->DataSize = pFifoBlockIn->DataSize;

         switch (pDevice->Acquisition1.Format)
         {
            case t_File::EWF:
               if (CONFIG(EwfFormat) == t_File::AEWF)
               {
                  CHK_EXIT (AewfPreprocess (&pFifoBlockOut->AewfPreprocess, pFifoBlockIn ->Buffer, pFifoBlockIn ->DataSize,
                                                                            pFifoBlockOut->Buffer, pFifoBlockOut->BufferSize))
                  if (pFifoBlockOut->AewfPreprocess.Compressed)
                  {
                     CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlockIn))
                  }
                  else
                  {
                     pFifoBlockIn->AewfPreprocess = pFifoBlockOut->AewfPreprocess;
                     CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlockOut))
                     pFifoBlockOut = pFifoBlockIn;
                  }
                  break;
               }
               #if (ENABLE_LIBEWF)
                  else
                  {
                     CompressedSize = CompressedMaxSize;  // Must be initialised with the max buffer size (we use this one instead of MULTITHREADED_COMPRESSION_FIFO_BLOCK_SIZE in order to check if it works as written in the zlib docu)
                     #if (LIBEWF_VERSION < 20130416)
                        rc = libewf_raw_write_prepare_buffer (pEwfHandle, pFifoBlockIn ->Buffer, pFifoBlockIn->DataSize,
                                                                         pFifoBlockOut->Buffer, &CompressedSize,
                                                                         &pFifoBlockOut->EwfCompressionUsed,
                                                                         &pFifoBlockOut->EwfChunkCRC,
                                                                         &pFifoBlockOut->EwfWriteCRC);
                     #else
                        rc = libewf_handle_prepare_write_chunk(pEwfHandle, pFifoBlockIn ->Buffer, pFifoBlockIn->DataSize,
                                                                           pFifoBlockOut->Buffer, &CompressedSize,
                                                                          &pFifoBlockOut->EwfCompressionUsed,
                                                                          &pFifoBlockOut->EwfChunkCRC,
                                                                          &pFifoBlockOut->EwfWriteCRC, nullptr);
                     #endif
                     if (pFifoBlockOut->EwfCompressionUsed)
                     {
                        pFifoBlockOut->EwfDataSize = CompressedSize;                        // Data to be forwarded is contained in
                        CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlockIn))     // pFifoBlockOut, pFifoBlockIn no longer needed.
                     }
                     else
                     {
                        pFifoBlockIn->EwfDataSize        = pFifoBlockIn ->DataSize;
                        pFifoBlockIn->EwfCompressionUsed = pFifoBlockOut->EwfCompressionUsed;
                        pFifoBlockIn->EwfChunkCRC        = pFifoBlockOut->EwfChunkCRC;
                        pFifoBlockIn->EwfWriteCRC        = pFifoBlockOut->EwfWriteCRC;
                        CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlockOut))    // No compression, we'll forward the
                        pFifoBlockOut = pFifoBlockIn;                                       // original block we received
                     }

                     if (rc != (ssize_t) pFifoBlockOut->EwfDataSize)
                     {
                        LOG_INFO ("[%s] libewf_raw_write_prepare_buffer returned %d. DataSize=%u, CompressedMaxSize=%zd, CompressedSize=%zd, EwfCompressionUsed=%d",
                                  QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice), (int)rc,
                                  pFifoBlockOut->DataSize, CompressedMaxSize, CompressedSize, pFifoBlockOut->EwfCompressionUsed)
                        CHK_EXIT (ERROR_THREADCOMPRESS_LIBEWF_FAILED)
                     }
                     pFifoBlockOut->EwfPreprocessed = true;
                  }
               #endif
               break;

            case t_File::AAFF:
               CHK_EXIT (AaffPreprocess (&pFifoBlockOut->AaffPreprocess, pFifoBlockIn->Buffer, pFifoBlockIn->DataSize, pFifoBlockOut->Buffer, pFifoBlockOut->BufferSize))
               if (pFifoBlockOut->AaffPreprocess.Compressed)
               {
                  CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlockIn))
               }
               else
               {
                  pFifoBlockIn->AaffPreprocess = pFifoBlockOut->AaffPreprocess;
                  CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlockOut))
                  pFifoBlockOut = pFifoBlockIn;
               }
               break;

            default:
               CHK_EXIT (ERROR_THREADCOMPRESS_INVALID_FORMAT)
         }
         CHK_EXIT (pFifoOut->Insert (pFifoBlockOut))
      }
      else
      {
         LOG_INFO ("[%s] Dummy block", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice))
         Finished = true;
      }
   }
   if ((pDevice->Acquisition1.Format == t_File::EWF) && (CONFIG(EwfFormat) != t_File::AEWF))
      CHK_EXIT (pDevice->pThreadWrite1->ReleaseFileHandle ())

   LOG_INFO ("[%s] Compression thread #%d exits now - %lld blocks processed", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice), pOwn->ThreadNr, Blocks)
   CHK_EXIT (SetDebugMessage ("Exit run function"))
}


void t_ThreadCompress::SlotFinished (void)
{
   emit SignalEnded (pOwn->pDevice, pOwn->ThreadNr);
}

