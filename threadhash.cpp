// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for calculating hashes
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
#include "threadhash.h"
#include "threadwrite.h"
#include "hash.h"

class t_ThreadHashLocal
{
   public:
      t_pDevice pDevice;
};

t_ThreadHash::t_ThreadHash(void)
{
   CHK_EXIT (ERROR_THREADHASH_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 not initialised


t_ThreadHash::t_ThreadHash (t_pDevice pDevice)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADHASH_CONSTRUCTOR_NOT_SUPPORTED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADHASH_LIBEWF_FAILED))
      Initialised = true;
   }

   pOwn = new t_ThreadHashLocal;
   pOwn->pDevice = pDevice;

   CHK_QT_EXIT (connect (this, SIGNAL(finished()), this, SLOT(SlotFinished())))
}

t_ThreadHash::~t_ThreadHash (void)
{
   delete pOwn;
}

void t_ThreadHash::run (void)
{
   t_pDevice          pDevice;
   t_pFifoBlock       pFifoBlock;
   bool                Finished;
   quint64           *pBlocks;
   quint64             BlocksCalculated = 0;
   quint64             BlocksVerified   = 0;
   quint64             Hashed           = 0;
   t_HashContextMD5    HashContextMD5;
   t_HashContextSHA1   HashContextSHA1;
   t_HashContextSHA256 HashContextSHA256;
   bool                VerifyLoop = false;

   LOG_INFO ("[%s] Hash thread started", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice))
   CHK_EXIT (SetDebugMessage ("Start run function"))
   pDevice = pOwn->pDevice;
   pBlocks = &BlocksCalculated;
   for (;;)  // The whole loop is done 2 times if the user chose to do a source verification, 1 time otherwise.
   {
      *pBlocks = 0;
      Finished = false;
      if (pDevice->Acquisition1.CalcMD5   ) CHK_EXIT (HashMD5Init    (&HashContextMD5   ))
      if (pDevice->Acquisition1.CalcSHA1  ) CHK_EXIT (HashSHA1Init   (&HashContextSHA1  ))
      if (pDevice->Acquisition1.CalcSHA256) CHK_EXIT (HashSHA256Init (&HashContextSHA256))
      do
      {
         CHK_EXIT (SetDebugMessage ("Wait for block"))
         CHK_EXIT (pDevice->pFifoHashIn->Get (pFifoBlock))
         CHK_EXIT (SetDebugMessage ("Process block"))
         if (pFifoBlock)
         {
            (*pBlocks)++;
            if (pDevice->Acquisition1.CalcMD5   ) CHK_EXIT (HashMD5Append    (&HashContextMD5   , pFifoBlock->Buffer, pFifoBlock->DataSize))
            if (pDevice->Acquisition1.CalcSHA1  ) CHK_EXIT (HashSHA1Append   (&HashContextSHA1  , pFifoBlock->Buffer, pFifoBlock->DataSize))
            if (pDevice->Acquisition1.CalcSHA256) CHK_EXIT (HashSHA256Append (&HashContextSHA256, pFifoBlock->Buffer, pFifoBlock->DataSize))
            if (VerifyLoop)
            {
               pDevice->CurrentVerifyPosSrc.Inc (pFifoBlock->DataSize);
               CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlock))
            }
            else
            {
               Hashed += pFifoBlock->DataSize;
               if (Hashed >= pDevice->Size)  // Make sure to calculate the hash before inserting the last block or else, if the write thread
               {                             // get a time slice in between, it could possibly put an invalid hash into the image file.
                  if (pDevice->Acquisition1.CalcMD5   ) CHK_EXIT (HashMD5Digest    (&HashContextMD5   , &pDevice->MD5Digest   ))
                  if (pDevice->Acquisition1.CalcSHA1  ) CHK_EXIT (HashSHA1Digest   (&HashContextSHA1  , &pDevice->SHA1Digest  ))
                  if (pDevice->Acquisition1.CalcSHA256) CHK_EXIT (HashSHA256Digest (&HashContextSHA256, &pDevice->SHA256Digest))
               }
               CHK_EXIT (pDevice->pFifoHashOut->Insert (pFifoBlock))
            }
         }
         else
         {
            LOG_INFO ("[%s] Dummy block", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice))
            Finished = true;
         }
      } while (!Finished && !pDevice->Error.Abort());
      if (pDevice->Error.Abort())
      {
         break;
      }
      else if (VerifyLoop)
      {
         if (pDevice->Acquisition1.CalcMD5   ) CHK_EXIT (HashMD5Digest    (&HashContextMD5   , &pDevice->MD5DigestVerifySrc   ))
         if (pDevice->Acquisition1.CalcSHA1  ) CHK_EXIT (HashSHA1Digest   (&HashContextSHA1  , &pDevice->SHA1DigestVerifySrc  ))
         if (pDevice->Acquisition1.CalcSHA256) CHK_EXIT (HashSHA256Digest (&HashContextSHA256, &pDevice->SHA256DigestVerifySrc))
         break;
      }
      else if (pDevice->Acquisition1.VerifySrc)
      {
         pBlocks = &BlocksVerified;
         VerifyLoop = true;
         CHK_EXIT (pDevice->pFifoHashOut->InsertDummy ())  // Make compression threads know they can end
      }
      else
      {
         break;
      }
   }

   if (pDevice->Acquisition1.VerifySrc)
        LOG_INFO ("[%s] Hash thread exits now - %lld blocks processed, %lld blocks verified", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice), BlocksCalculated, BlocksVerified)
   else LOG_INFO ("[%s] Hash thread exits now - %lld blocks processed"                      , QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice), BlocksCalculated)
   CHK_EXIT (SetDebugMessage ("Exit run function"))
}

void t_ThreadHash::SlotFinished (void)
{
   emit SignalEnded (pOwn->pDevice);
}

