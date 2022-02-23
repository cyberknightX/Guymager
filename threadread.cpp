// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for reading data.
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

#include <errno.h>
#include <unistd.h>

#include <QtCore>

#include "common.h"
#include "config.h"
#include "device.h"
#include "threadread.h"
#include "threadwrite.h"

#include "fcntl.h"
//#define _GNU_SOURCE

const int THREADREAD_CHECK_CONNECTED_SLEEP = 1000;
const int THREADREAD_SLOWDOWN_SLEEP        =  700;


class t_ThreadReadLocal
{
   public:
      t_pDevice  pDevice;
      bool      *pSlowDownRequest;
};

t_ThreadRead::t_ThreadRead(void)
{
   CHK_EXIT (ERROR_THREADREAD_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 not initialised

t_ThreadRead::t_ThreadRead (t_pDevice pDevice, bool *pSlowDownRequest)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_FILESRC_ALREADY_OPEN     ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_NO_DATA                  ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_DEVICE_DISCONNECTED      ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_UNEXPECTED_FAILURE       ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_CONSTRUCTOR_NOT_SUPPORTED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_BLOCKSIZE                ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_DEVICE_ABORTED           ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_LIBEWF_FAILED            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADREAD_BAD_FILE_HANDLE          ))
      Initialised = true;
   }

   pOwn = new t_ThreadReadLocal;
   pOwn->pDevice          = pDevice;
   pOwn->pSlowDownRequest = pSlowDownRequest;

   CHK_QT_EXIT (connect (this, SIGNAL(finished()), this, SLOT(SlotFinished())))
}

t_ThreadRead::~t_ThreadRead (void)
{
   delete pOwn;
}

inline bool ThreadReadEOF (t_pDevice pDevice)
{
//   LOG_INFO ("EOF check %lld %lld", pDevice->CurrentPos, pDevice->Size)
   return (pDevice->CurrentReadPos.Get() >= pDevice->Size);
}

static APIRET ThreadReadCheckDeviceExists (t_pcDevice pDevice, bool &DeviceExists)
{
   FILE *pFileTest;

   pFileTest = fopen64 (QSTR_TO_PSZ(pDevice->LinuxDevice), "r");
   DeviceExists = (pFileTest != nullptr);
   if (DeviceExists)
      (void) fclose (pFileTest);

   return NO_ERROR;
}

static APIRET ThreadReadDeviceDisconnected (t_pDevice pDevice, const char* pStrAction)
{
   if (pDevice->FileDescSrc != t_Device::FileDescEmpty)
   {
      (void) close (pDevice->FileDescSrc);
      pDevice->FileDescSrc = t_Device::FileDescEmpty;
   }
   if (pDevice->GetState() == t_Device::Launch)
   {
      LOG_INFO ("[%s] Device disconnected just after acquisition launch (%s, %d, %s)", QSTR_TO_PSZ(pDevice->LinuxDevice), pStrAction, errno, strerror(errno))
      pDevice->SetState (t_Device::LaunchPaused); // Only switch statem, but do not set error here (will be done in t_MainWindow::SlotScanFinished)
   }
   else
   {
      if (pDevice->GetState() == t_Device::Acquire)
           pDevice->SetState (t_Device::AcquirePaused);
      else pDevice->SetState (t_Device::VerifyPaused );
      LOG_INFO ("[%s] Device disconnected (%s, %d, %s), switching device state to %s", QSTR_TO_PSZ(pDevice->LinuxDevice), pStrAction, errno, strerror(errno), pDevice->StateStr())
   }
   return ERROR_THREADREAD_DEVICE_DISCONNECTED;
}

static APIRET ThreadReadAdjustSeek (t_pDevice pDevice)        // According to the GNU Clib docu, a seek should be done after read errors
{
   off64_t rc;

   if (pDevice->FileDescSrc == t_Device::FileDescEmpty)
      CHK (ERROR_THREADREAD_FILESRC_NOT_OPEN)

   if (ThreadReadEOF (pDevice))
      return NO_ERROR;

   rc = lseek64 (pDevice->FileDescSrc, (off64_t)pDevice->CurrentReadPos.Get(), SEEK_SET);
   if (rc < 0)
      CHK_RET (ThreadReadDeviceDisconnected (pDevice, "seek"))

   return NO_ERROR;
}


static APIRET ThreadReadBlock0 (t_pDevice pDevice, unsigned char *pBuffer, unsigned int Size)
{
   bool    DeviceExists;
   ssize_t Read;
   int     OpenFlags;

   errno = 0;
   if (pDevice->FileDescSrc == t_Device::FileDescEmpty)
   {
      OpenFlags = O_RDONLY | O_NOATIME;
      if ((CONFIG (DirectIO) || pDevice->FallbackMode) && // Try DirectIO if configured to do so; Also use DirectIO in FallBack mode, in order to avoid the "contagious
           CONFIG (FifoMemoryManager))                    // error". For DirectIO, FifoMemoryManager must be on, as the buffers must be located on page size (4K) boundaries
      {                                                   // (this only is guaranteed when working with Guymager's internal memory manager (see t_FifoMemory::t_FifoMemory))
         LOG_INFO ("[%s] Trying direct mode", QSTR_TO_PSZ(pDevice->LinuxDevice))
         OpenFlags |= O_DIRECT;
      }

      pDevice->FileDescSrc = open64 (QSTR_TO_PSZ(pDevice->LinuxDevice), OpenFlags);
      if ((pDevice->FileDescSrc < 0) && (OpenFlags & O_DIRECT) && (errno == EINVAL))   // If an invalid parameter is reported, O_DIRECT might have
      {                                                                                // caused problems. This was observed when opening files
         OpenFlags &= ~O_DIRECT;                                                       // (added as special device) on the cases server (dd reported
         pDevice->FileDescSrc = open64 (QSTR_TO_PSZ(pDevice->LinuxDevice), OpenFlags); // the same error with iflag=direct). Try again without it.
      }
      if ((pDevice->FileDescSrc < 0) && (OpenFlags & O_NOATIME))                       // Another cause for trouble might be NOATIME. This was observed when
      {                                                                                // giving standard users read access to physical disks).
         OpenFlags &= ~O_NOATIME;
         pDevice->FileDescSrc = open64 (QSTR_TO_PSZ(pDevice->LinuxDevice), OpenFlags);
      }
      LOG_INFO ("[%s] Flag DIRECT  on source device switched %s", QSTR_TO_PSZ(pDevice->LinuxDevice), (OpenFlags & O_DIRECT ) ? "on":"off")
      LOG_INFO ("[%s] Flag NOATIME on source device switched %s", QSTR_TO_PSZ(pDevice->LinuxDevice), (OpenFlags & O_NOATIME) ? "on":"off")

      if (pDevice->FileDescSrc < 0)
         CHK_RET (ThreadReadDeviceDisconnected (pDevice, "open"))
      CHK_RET (ThreadReadAdjustSeek (pDevice))
   }

   Read = read (pDevice->FileDescSrc, pBuffer, Size);

// Bad sector simulation
// if ((pDevice->CurrentReadPos.Get() > 900000000) && (pDevice->CurrentReadPos.Get() < (900000000+65536*2)))
// {
//    #warning "bad sector simulation is still on"
//    Read=0;
// }
   if (Read == (ssize_t)Size)
   {
      pDevice->CurrentReadPos.Inc(Size);
      return NO_ERROR;
   }
   else
   {
      CHK (ThreadReadCheckDeviceExists (pDevice, DeviceExists))
      if (DeviceExists)
      {
         CHK_RET (ThreadReadAdjustSeek (pDevice))
         return ERROR_THREADREAD_NO_DATA;
      }
      else
      {
         CHK_RET (ThreadReadDeviceDisconnected (pDevice, "read"))
      }
   }

   return NO_ERROR;
}


APIRET t_ThreadRead::ThreadReadBlock (t_pFifoBlock &pFifoBlock)
{
   t_pDevice      pDevice;
   unsigned char *pBuffer;
   quint64         BytesToRead;
   quint64         BytesRead = 0;
   quint64         RemainingSize;
   quint64         Sector;
   quint64         BadSectorCount;
   quint64         PrevReadPos;
   unsigned int    ReadTry;
   APIRET          RcRead;
   APIRET          RcSeek;

   pDevice       = pOwn->pDevice;
   PrevReadPos   = pDevice->CurrentReadPos.Get();
   RemainingSize = pDevice->Size - PrevReadPos;
   if ((quint64)pDevice->FifoBlockSize > RemainingSize)
        BytesToRead = RemainingSize;
   else BytesToRead = pDevice->FifoBlockSize;

   if (BytesToRead > INT_MAX)
      CHK (ERROR_THREADREAD_BLOCKSIZE)

   CHK (t_Fifo::Create (pDevice->pFifoMemory, pFifoBlock, pDevice->FifoAllocBlockSize))

   pFifoBlock->DataSize = (unsigned int) BytesToRead;
   pBuffer = pFifoBlock->Buffer;

   if (pDevice->FallbackMode)
        ReadTry = pDevice->SectorSize;
   else ReadTry = BytesToRead;
   do
   {
      RcRead = ThreadReadBlock0 (pDevice, pBuffer, ReadTry);
      switch (RcRead)
      {
         case NO_ERROR:
            BytesRead += ReadTry;
            pBuffer   += ReadTry;
            break;

         case ERROR_THREADREAD_NO_DATA:
            if (!pDevice->FallbackMode)
            {
               pDevice->FallbackMode = true;
               if (!CONFIG(DirectIO))                             // When running without DirectIO we close the source device,
               {                                                  // thus forcing ThreadReadBlock0 to re-open it with DirectIO
                  (void) close (pDevice->FileDescSrc);            // switched on (in order to avoid the "contagious error")
                  pDevice->FileDescSrc = t_Device::FileDescEmpty;
               }
               ReadTry = pDevice->SectorSize;
               LOG_INFO ("[%s] Device read error, switching to slow fallback mode. Reading sectors individually from now on.", QSTR_TO_PSZ(pDevice->LinuxDevice))
            }
            else
            {
               Sector = pDevice->CurrentReadPos.Get() / pDevice->SectorSize;
               pDevice->CurrentReadPos.Inc(ReadTry);
               RcSeek = ThreadReadAdjustSeek (pDevice); // If the seek still succeeds, then there's probably just a bad sector on
               switch (RcSeek)                          // the source device, let's replace it by a zero sector.
               {
                  case NO_ERROR:
                     if ((pDevice->Acquisition1.Format == t_File::AAFF) && (CONFIG (AffMarkBadSectors)))
                          CHK (AaffCopyBadSectorMarker (pBuffer, ReadTry))
                     else memset (pBuffer, 0, ReadTry);
                     BytesRead += ReadTry;
                     pBuffer   += ReadTry;

                     pDevice->AddBadSector (Sector);
                     BadSectorCount = pDevice->GetBadSectorCount (false);
                     if ( CONFIG(BadSectorLogThreshold) == 0                          ||  // Threshold feature disabled if cfg param is zero
                          (BadSectorCount <= (quint64) CONFIG(BadSectorLogThreshold)) ||
                         ((BadSectorCount %  (quint64) CONFIG(BadSectorLogModulo   )) == 0))
                        LOG_INFO ("[%s] Read error on sector %lld, bad data replaced by zero block (%lld bad sectors up until now)", QSTR_TO_PSZ(pDevice->LinuxDevice), Sector, BadSectorCount)
                     if (BadSectorCount == (quint64)CONFIG(BadSectorLogThreshold))
                        LOG_INFO ("[%s] Bad sector threshold has been reached, only logging every 1000th bad sector from now on.", QSTR_TO_PSZ(pDevice->LinuxDevice))

                     if (CONFIG(JobMaxBadSectors) &&
                        (CONFIG(LimitJobs) != CONFIG_LIMITJOBS_OFF) &&
                        (pDevice->GetBadSectorCount(false) > (quint64)CONFIG(JobMaxBadSectors)))
                        pDevice->Error.Set (t_Device::t_Error::TooManyBadSectors);

                     break;
                  case ERROR_THREADREAD_DEVICE_DISCONNECTED:
                     break;
                  default:
                     CHK (RcSeek)
               }
            }
            break;

         case ERROR_THREADREAD_DEVICE_DISCONNECTED:
            break;

         default:
            CHK (RcRead)
      }
   } while ((BytesRead < BytesToRead) && (pDevice->GetState() != t_Device::LaunchPaused )
                                      && (pDevice->GetState() != t_Device::AcquirePaused)
                                      && (pDevice->GetState() != t_Device::VerifyPaused )
                                      && !pDevice->Error.Abort());

   if ((pDevice->GetState() == t_Device::LaunchPaused ) ||
       (pDevice->GetState() == t_Device::AcquirePaused) ||
       (pDevice->GetState() == t_Device::VerifyPaused ))
   {
      CHK (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlock))
      pDevice->CurrentReadPos.Set(PrevReadPos);

      return ERROR_THREADREAD_DEVICE_DISCONNECTED;
   }
   else if (pDevice->Error.Abort())
   {
      CHK (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlock))

      return ERROR_THREADREAD_DEVICE_ABORTED;
   }
   else if ((pDevice->FallbackMode) && (RcRead == NO_ERROR))
   {
      pDevice->FallbackMode = false;  // Switch to fast reading mode if last read was good
      LOG_INFO ("[%s] Device read ok again, switching to fast block read.", QSTR_TO_PSZ(pDevice->LinuxDevice))
      if (!CONFIG(DirectIO))                                  // Be sure to switch DirectIO off again if configured so.
      {                                                       // We do so by closing the source device. ThreadReadBlock0
         (void) close (pDevice->FileDescSrc);                 // is then forced to re-open the device where it will switch
         pDevice->FileDescSrc = t_Device::FileDescEmpty;      // off DirectIO.
      }
   }

   pFifoBlock->LastBlock = ThreadReadEOF (pDevice);

   return NO_ERROR;
}

void t_ThreadRead::run (void)
{
   t_pDevice          pDevice;
   t_pFifoBlock       pFifoBlock;
   APIRET              rc;
   int                 rcf;
   bool                CalcHashMD5;
   bool                CalcHashSHA1;
   bool                CalcHashSHA256;
   quint64           *pBlocks;
   quint64             BlocksRead     = 0;
   quint64             BlocksVerified = 0;
   quint64             BytesRead      = 0;
   quint64             BytesVerified  = 0;
   t_HashContextMD5    HashContextMD5;
   t_HashContextSHA1   HashContextSHA1;
   t_HashContextSHA256 HashContextSHA256;

   LOG_INFO ("[%s] Read thread started", QSTR_TO_PSZ (pOwn->pDevice->LinuxDevice))
   CHK_EXIT (SetDebugMessage ("Start run function"))
   pDevice = pOwn->pDevice;
   CalcHashMD5    = pDevice->Acquisition1.CalcMD5    && !CONFIG (UseSeparateHashThread);
   CalcHashSHA1   = pDevice->Acquisition1.CalcSHA1   && !CONFIG (UseSeparateHashThread);
   CalcHashSHA256 = pDevice->Acquisition1.CalcSHA256 && !CONFIG (UseSeparateHashThread);

   pBlocks = &BlocksRead;

//   pDevice->AddBadSector (3);   // Simulate bad sectors
//   pDevice->AddBadSector (4);
//   pDevice->AddBadSector (5);
//   pDevice->AddBadSector (122);

   for (;;)
   {
      pDevice->CurrentReadPos.Zero();
      pDevice->FileDescSrc   = t_Device::FileDescEmpty;
      pDevice->FallbackMode  = false;
      *pBlocks = 0;
      if (CalcHashMD5)    CHK_EXIT (HashMD5Init    (&HashContextMD5   ))
      if (CalcHashSHA1)   CHK_EXIT (HashSHA1Init   (&HashContextSHA1  ))
      if (CalcHashSHA256) CHK_EXIT (HashSHA256Init (&HashContextSHA256))
      do
      {
         if (*(pOwn->pSlowDownRequest))
            msleep (THREADREAD_SLOWDOWN_SLEEP);

         CHK_EXIT (SetDebugMessage ("Calling ThreadReadBlock"))
         rc = ThreadReadBlock (pFifoBlock);
         CHK_EXIT (SetDebugMessage ("ThreadReadBlock returned"))
         switch (rc)
         {
            case NO_ERROR:
               pFifoBlock->Nr = (*pBlocks);

               if (CalcHashMD5)    CHK_EXIT (HashMD5Append    (&HashContextMD5   , pFifoBlock->Buffer, pFifoBlock->DataSize))
               if (CalcHashSHA1)   CHK_EXIT (HashSHA1Append   (&HashContextSHA1  , pFifoBlock->Buffer, pFifoBlock->DataSize))
               if (CalcHashSHA256) CHK_EXIT (HashSHA256Append (&HashContextSHA256, pFifoBlock->Buffer, pFifoBlock->DataSize))

               if ((pDevice->GetState() == t_Device::Verify) && (CalcHashMD5 || CalcHashSHA1 || CalcHashSHA256))
               {
                  pDevice->CurrentVerifyPosSrc.Inc (pFifoBlock->DataSize);
                  CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlock))
               }
               else
               {
                  CHK_EXIT (pDevice->pFifoRead->Insert (pFifoBlock))
               }
               (*pBlocks)++;
               break;

            case ERROR_THREADREAD_DEVICE_DISCONNECTED:
               while (!pDevice->Error.Abort() && ((pDevice->GetState() == t_Device::LaunchPaused ) ||
                                                  (pDevice->GetState() == t_Device::AcquirePaused) ||
                                                  (pDevice->GetState() == t_Device::VerifyPaused )) )
                  msleep (THREADREAD_CHECK_CONNECTED_SLEEP);
               break;

            case ERROR_THREADREAD_DEVICE_ABORTED:
               break;
            default:
               LOG_ERROR ("[%s] Unexpected return code from ThreadReadBlock", QSTR_TO_PSZ(pDevice->LinuxDevice))
               CHK_EXIT (rc)
         }
      } while (!ThreadReadEOF(pDevice) && !pDevice->Error.Abort());

      if (pDevice->Error.Abort())
         break;

      if ((pDevice->GetState() == t_Device::Acquire) ||
          (pDevice->GetState() == t_Device::Launch))
           BytesRead     = pDevice->CurrentReadPos.Get();
      else BytesVerified = pDevice->CurrentReadPos.Get();

      if (pDevice->GetState() == t_Device::Verify)
      {
         if (CalcHashMD5)    CHK_EXIT (HashMD5Digest    (&HashContextMD5   , &pDevice->MD5DigestVerifySrc   ))
         if (CalcHashSHA1)   CHK_EXIT (HashSHA1Digest   (&HashContextSHA1  , &pDevice->SHA1DigestVerifySrc  ))
         if (CalcHashSHA256) CHK_EXIT (HashSHA256Digest (&HashContextSHA256, &pDevice->SHA256DigestVerifySrc))
         LOG_INFO ("[%s] Source verification completed", QSTR_TO_PSZ (pDevice->LinuxDevice))
         break;
      }

      if (CalcHashMD5)    CHK_EXIT (HashMD5Digest    (&HashContextMD5   , &pDevice->MD5Digest   ))
      if (CalcHashSHA1)   CHK_EXIT (HashSHA1Digest   (&HashContextSHA1  , &pDevice->SHA1Digest  ))
      if (CalcHashSHA256) CHK_EXIT (HashSHA256Digest (&HashContextSHA256, &pDevice->SHA256Digest))

      if ((pDevice->Acquisition1.VerifySrc) ||
          (pDevice->Acquisition1.VerifyDst) ||
          (pDevice->Acquisition2.VerifyDst && pDevice->Duplicate))
      {
         pDevice->SetState (t_Device::Verify);
         pDevice->StartTimestampVerify = QDateTime::currentDateTime();
      }

      if (pDevice->Acquisition1.VerifySrc)
      {
         (void) close (pDevice->FileDescSrc);
         pDevice->FileDescSrc = t_Device::FileDescEmpty;
         LOG_INFO ("[%s] All data has been read, now re-reading for source verification", QSTR_TO_PSZ (pDevice->LinuxDevice))
         pBlocks = &BlocksVerified;
//         if (CONFIG (UseSeparateHashThread))
         CHK_EXIT (pDevice->pFifoRead->InsertDummy ())
      }
      else
      {
         LOG_INFO ("[%s] All data has been read", QSTR_TO_PSZ (pDevice->LinuxDevice))
         break;
      }
   }

   if (pDevice->FileDescSrc != t_Device::FileDescEmpty)
   {
      rcf = close (pDevice->FileDescSrc);
      pDevice->FileDescSrc = t_Device::FileDescEmpty;
      if (rcf)
      {
         LOG_INFO ("[%s] Unexpected error on close, errno = %d", QSTR_TO_PSZ(pDevice->LinuxDevice), errno)
         pDevice->Error.Set(t_Device::t_Error::ReadError);
      }
   }

   LOG_INFO ("[%s] Read thread exits now (%lld blocks read, %llu bytes)", QSTR_TO_PSZ (pDevice->LinuxDevice), BlocksRead, BytesRead)
   if (pDevice->Acquisition1.VerifySrc)
      LOG_INFO ("[%s]   Source verification: %lld blocks read, %llu bytes)", QSTR_TO_PSZ(pDevice->LinuxDevice), BlocksVerified, BytesVerified)
   CHK_EXIT (SetDebugMessage ("Exit run function"))
}

void t_ThreadRead::SlotFinished (void)
{
   emit SignalEnded (pOwn->pDevice);
}

