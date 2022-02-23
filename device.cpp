// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The DeviceList represents the central data structure of
//                  the application. Its contents are displayed in the main
//                  window.
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

#include <float.h>

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif
#include <QString>
#include <QList>

#include "toolconstants.h"

#include "common.h"
#include "qtutil.h"
#include "util.h"
#include "config.h"
#include "mainwindow.h"
#include "main.h"
#include "compileinfo.h"

// ---------------------------
//         t_Error
// ---------------------------

t_Device::t_Error::t_Error (t_pcDevice pDevice) :
   poDevice (pDevice)
{
   Clear();
};

void t_Device::t_Error::Set (uint Err, int WriteInstance)
{
   switch (WriteInstance)
   {
      case 0 : if (Err >= t_Error::CommonMax) CHK_EXIT (ERROR_DEVICE_INVALID_ERROR); oErrorCommon = Err; break;
      case 1 : if (Err <= t_Error::CommonMax) CHK_EXIT (ERROR_DEVICE_INVALID_ERROR); oErrorWrite1 = Err; break;
      case 2 : if (Err <= t_Error::CommonMax) CHK_EXIT (ERROR_DEVICE_INVALID_ERROR); oErrorWrite2 = Err; break;
      default: CHK_EXIT (ERROR_DEVICE_INVALID_ERROR)
   }

   if (poDevice->Duplicate)
        oAbort = oErrorCommon || (oErrorWrite1 && oErrorWrite2);
   else oAbort = oErrorCommon ||  oErrorWrite1;
}

uint t_Device::t_Error::Get (int WriteInstance) const
{
   switch (WriteInstance)
   {
      case 0 : return oErrorCommon;
      case 1 : return oErrorWrite1;
      case 2 : return oErrorWrite2;
      default: CHK_EXIT (ERROR_DEVICE_INVALID_ERROR)
   }
   return 0;
}

bool t_Device::t_Error::Abort (int WriteInstance) const
{
   switch (WriteInstance)
   {
      case 0 : return oAbort;
      case 1 : return oAbort || (oErrorWrite1 != 0);
      case 2 : return oAbort || (oErrorWrite2 != 0);
      default: CHK_EXIT (ERROR_DEVICE_INVALID_ERROR)
   }
   return oAbort;
}

void t_Device::t_Error::Clear (void)
{
   oErrorCommon = None;
   oErrorWrite1 = None;
   oErrorWrite2 = None;
   oAbort = false;
}

QString t_Device::GetErrorText (void) const
{
   QString Text;

   switch (Error.Get())
   {
      case t_Error::UserRequest           : Text = t_MainWindow::tr("Aborted by user");           break;
      case t_Error::AcquisitionStartFailed: Text = t_MainWindow::tr("Acquisition start failure"); break;
      case t_Error::DeviceMovedOnLaunch   : Text = t_MainWindow::tr("Device (re)moved");          break;
      case t_Error::ReadError             : Text = t_MainWindow::tr("Device read error");         break;
      case t_Error::TooManyBadSectors     : Text = t_MainWindow::tr("Too many bad sectors");      break;
      case t_Error::DisconnectTimeout     : Text = t_MainWindow::tr("Disconnect timeout");        break;
      case t_Error::None:
         if (!Duplicate)
         {
            switch (Error.Get(1))
            {
               case t_Error::WriteError : Text = t_MainWindow::tr("Image write error" ); break;
               case t_Error::VerifyError: Text = t_MainWindow::tr("Image verify error"); break;
               case t_Error::InfoError  : Text = t_MainWindow::tr("Info file write error"  ); break;
            }
         }
         else
         {
            uint Err1 = Error.Get(1);
            uint Err2 = Error.Get(2);

            switch (Err1)
            {
               case t_Error::WriteError : Text = t_MainWindow::tr("Image 1 write error" ); break;
               case t_Error::VerifyError: Text = t_MainWindow::tr("Image 1 verify error"); break;
               case t_Error::InfoError  : Text = t_MainWindow::tr("Info file 1 write error"  ); break;
            }
            if (Err1 && Err2)
               Text += " - ";
            switch (Err2)
            {
               case t_Error::WriteError : Text += t_MainWindow::tr("Image 2 write error" ); break;
               case t_Error::VerifyError: Text += t_MainWindow::tr("Image 2 verify error"); break;
               case t_Error::InfoError  : Text += t_MainWindow::tr("Info file 2 write error"  ); break;
            }
         }
         break;
   }
   return Text;
}

// ---------------------------
//         t_Device
// ---------------------------

static const int DEVICE_MIN_RUNNING_SECONDS = 20;  // The minmum number of acquisition seconds before any estimation can be made

void t_Device::Initialise (void)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DEVICE_SERNR_MATCH_MODEL_MISMATCH ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DEVICE_SERNR_MATCH_LENGTH_MISMATCH))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DEVICE_BAD_STATE                  ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DEVICE_BAD_ABORTREASON            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DEVICE_NOT_CLEAN                  ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DEVICE_INVALID_MEDIAINFOSTATE     ))
      Initialised = true;
   }

//   this->SerialNumber      = QString();
//   this->LinuxDevice       = QString();
//   this->Model             = QString();
//   this->NativePath        = QString();
//   this->ByPath            = QString();
//   this->Interface         = QString();
   this->Local                = false;
   this->SectorSize           = 0;
   this->SectorSizePhys       = 0;
   this->Size                 = 0;
   this->SpecialDevice        = false;
   this->Removable            = false;

   this->State                = Idle;
//   this->Acquisition.Message       = QString();
//   this->Abort;
   this->AbortCount           = 0;
   this->DeleteAfterAbort     = false;
   this->Duplicate            = false;
   this->pFifoMemory          = nullptr;
   this->pFifoRead            = nullptr;
   this->pThreadRead          = nullptr;
   this->FileDescSrc          = t_Device::FileDescEmpty;
//   this->CurrentReadPos
//   this->BadSectors
//   this->BadSectorsVerify
   this->FallbackMode         = false;
   this->StartTimestamp       = QDateTime();
   this->StartTimestampVerify = QDateTime();
   this->StopTimestamp        = QDateTime();

   this->pThreadWrite1        = nullptr;
   this->pThreadWrite2        = nullptr;
//   this->CurrentVerifyPosSrc
   this->pFifoWrite1          = nullptr;
   this->pFifoWrite2          = nullptr;

   this->pThreadHash          = nullptr;
   this->pFifoHashIn          = nullptr;
   this->pFifoHashOut         = nullptr;
   memset (&this->MD5Digest            , 0, sizeof(this->MD5Digest            ));
   memset (&this->MD5DigestVerifySrc   , 0, sizeof(this->MD5DigestVerifySrc   ));
   memset (&this->SHA1Digest           , 0, sizeof(this->SHA1Digest           ));
   memset (&this->SHA1DigestVerifySrc  , 0, sizeof(this->SHA1DigestVerifySrc  ));
   memset (&this->SHA256Digest         , 0, sizeof(this->SHA256Digest         ));
   memset (&this->SHA256DigestVerifySrc, 0, sizeof(this->SHA256DigestVerifySrc));
// this->ImageFileHashes

   this->pFifoCompressIn      = nullptr;
   this->pFifoCompressOut     = nullptr;
   this->FifoMaxBlocks        = 0;
   this->FifoBlockSize        = 0;

//   this->Acquisition1
//   this->Acquisition2

   this->AddStateInfo.CanBeAcquired = true   ;
   this->AddStateInfo.Color         = COLOR_DEFAULT;
// this->AddStateInfo.Info          = QString();

   this->PrevTimestamp = QTime();
   this->PrevSpeed     = 0.0;
   this->Checked       = false;
   this->AddedNow      = false;
}

void t_Device::InitialiseDeviceSpecific (const QString &SerialNumber, const QString &LinuxDevice, const QString &Model,
                                         quint64 SectorSize, quint64 SectorSizePhys, quint64 Size,
                                         const QString &NativePath, const QString &ByPath, const QString &Interface)

{                                        //lint !e578: Declaration of symbol 'Xxx' hides symbol 't_Device::Xxx'
   this->SerialNumber   = SerialNumber;
   this->LinuxDevice    = LinuxDevice;
//   this->LinuxDevice    = "/data/virtualbox/knoppicilin.iso";
   this->Model          = Model;

   this->SectorSize     = SectorSize;
   this->SectorSizePhys = SectorSizePhys;
   this->Size           = Size;
   this->NativePath     = NativePath;
   this->ByPath         = ByPath;
   this->Interface      = Interface;

//   #define FAKE_DEVICE_SIZE
   #ifdef FAKE_DEVICE_SIZE
   #warning "Debug code for device size faking still enabled"
      // this->Size = GETMIN ( 10*1024, Size);               // 10K
      // this->Size = GETMIN ( 20971520ULL, Size);           // 20 MiB
      // this->Size = GETMIN ( 1024*1024*1024ULL, Size);     //  1 GB
       this->Size = GETMIN (32*1024*1024*1024ULL, Size);         // 32 GiB
      // this->Size = 3ULL*1024ULL*1024ULL*1024ULL*1024ULL;  //  3 TB
      printf ("\nSize of %s set to %llu bytes", QSTR_TO_PSZ(this->LinuxDevice), this->Size);
   #endif
}

t_Device::t_Device (const QString &SerialNumber, const QString &LinuxDevice, const QString &Model,
                    quint64 SectorSize, quint64 SectorSizePhys, quint64 Size,
                    const QString &NativePath,
                    const QString &ByPath,
                    const QString &Interface) :
   Error        (this),
   Acquisition1 (this, 1),
   Acquisition2 (this, 2)
{                   //lint !e578: Declaration of symbol 'Xxx' hides symbol 't_Device::Xxx'
   Initialise ();
   InitialiseDeviceSpecific (SerialNumber, LinuxDevice, Model, SectorSize, SectorSizePhys, Size, NativePath, ByPath, Interface);
}

t_Device::t_Device (const QString &SerialNumber, const PedDevice *pPedDev) :
   Error        (this),
   Acquisition1 (this, 1),
   Acquisition2 (this, 2)
{                   //lint !e578: Declaration of symbol 'Xxx' hides symbol 't_Device::Xxx'
   Initialise ();
   InitialiseDeviceSpecific (SerialNumber, pPedDev->path, pPedDev->model,
                                           (quint64) pPedDev->sector_size, (quint64) pPedDev->phys_sector_size,
                                           (quint64) pPedDev->length * (quint64) pPedDev->sector_size, NativePath, ByPath, Interface);
}

t_Device::~t_Device()
{
   if (pFifoRead     || pThreadRead   ||
       pFifoHashIn   || pThreadHash   ||
       pFifoHashOut  ||
       pFifoWrite1   || pThreadWrite1 ||
       pFifoWrite2   || pThreadWrite2 || (FileDescSrc != t_Device::FileDescEmpty))
      CHK_EXIT (ERROR_DEVICE_NOT_CLEAN)
}

bool t_Device::HasHashThread (void) const
{
   return CONFIG (UseSeparateHashThread) && (Acquisition1.CalcMD5  ||
                                             Acquisition1.CalcSHA1 ||
                                             Acquisition1.CalcSHA256);
}

bool t_Device::HasCompressionThreads (void) const
{
   return (CONFIG (CompressionThreads) > 0) && ((Acquisition1.Format == t_File::EWF ) ||
                                                (Acquisition1.Format == t_File::AAFF) ||
                                                (Acquisition1.Format == t_File::AEWF));
}

bool t_Device::HasActiveAcquistion (void) const
{
   return (State != Idle   ) && (State != Finished) &&
          (State != Aborted) && (State != Queued  );
}

bool t_Device::DisconnectTimeoutReached() const
{
   bool Reached = false;

   if (CONFIG(LimitJobs) != CONFIG_LIMITJOBS_OFF)
   {
      if ((GetState() == t_Device::AcquirePaused) && (DisconnectTime > 0))
         Reached = (time(nullptr)-DisconnectTime) >= CONFIG(JobDisconnectTimeout);
   }
   return Reached;
}

QVariant t_Device::GetUserField (t_pDevice pDevice)
{
   QString Val;

   Val = pDevice->Acquisition1.UserField;
   if (pDevice->Duplicate)
      Val += " / " + pDevice->Acquisition2.UserField;

   return Val;
}

QVariant t_Device::GetExaminer (t_pDevice pDevice)
{
   return pDevice->Acquisition1.Examiner;
}

//lint -save -e818 pDevice could have declared as pointing to const

QVariant t_Device::GetSerialNumber (t_pDevice pDevice)
{
   return pDevice->SerialNumber;
}

QVariant t_Device::GetAddStateInfo (t_pDevice pDevice)
{
   return pDevice->AddStateInfo.Info;
}

QVariant t_Device::GetLinuxDevice (t_pDevice pDevice)
{
   return pDevice->LinuxDevice;
}

QVariant t_Device::GetNativePath (t_pDevice pDevice)
{
   return pDevice->NativePath;
}

QVariant t_Device::GetByPath (t_pDevice pDevice)
{
   return pDevice->ByPath;
}

QVariant t_Device::GetInterface (t_pDevice pDevice)
{
   return pDevice->Interface;
}

QVariant t_Device::GetModel (t_pDevice pDevice)
{
   return pDevice->Model;
}

QVariant t_Device::GetState (t_pDevice pDevice)
{
   QString StrState;
   bool    HasVerifications;

   switch (pDevice->State)
   {
      case Idle         : if (pDevice->Local)
                          {
                             StrState = t_MainWindow::tr("Local device");
                          }
                          else
                          {
                             if (MainGetDeviceList()->UsedAsCloneDestination(pDevice))
                             {
                                StrState = t_MainWindow::tr("Used in clone operation");
                             }
                             else
                             {
                                StrState = t_MainWindow::tr("Idle");
                                if (CONFIG (CommandGetAddStateInfo)[0])
                                   StrState += " - " + pDevice->AddStateInfo.Info;
                             }
                          }
                          break;
      case Queued       : StrState = t_MainWindow::tr("Queued");
                          break;
      case Launch:
      case LaunchPaused :
      case Acquire      :
      case Verify       : if (pDevice->Error.Abort())
                          {
                             StrState = t_MainWindow::tr("Aborting..."        );
                          }
                          else
                          {
                             switch (pDevice->State)
                             {
                                case LaunchPaused: StrState = t_MainWindow::tr("Launched, device disconnected" ); break;
                                case Launch:       StrState = t_MainWindow::tr("Launched" ); break;
                                case Acquire:      StrState = t_MainWindow::tr("Running"  ); break;
                                default:           StrState = t_MainWindow::tr("Verifying");
                             }
                             if (pDevice->Duplicate)
                             {
                                if (pDevice->Error.Get(1) != t_Device::t_Error::None) StrState += " - image 1 failed, continuing with image 2";
                                if (pDevice->Error.Get(2) != t_Device::t_Error::None) StrState += " - image 2 failed, continuing with image 1";
                             }
                          }

                          if (pDevice->Error.Abort())
                          {
                             switch (pDevice->AbortCount)
                             {
                                case  0:
                                case  1: break;
                                case  2: StrState += " please be patient"; break;
                                case  3: StrState += " just keep cool"   ; break;
                                case  4: StrState += " I said KEEP COOL!"; break;
                                case  5: StrState += " you start annoying me"; break;
                                case  6: StrState += " just shut up"; break;
                                case  7: StrState += " SHUT UP!"; break;
                                case  8: StrState += " you want a segmentation fault?"; break;
                                case  9: StrState += " you can have one if you want"; break;
                                case 10: StrState += " I warn you"; break;
                                case 11: StrState += " last warning"; break;
                                case 12: StrState += " ultimate warning"; break;
                                case 13: StrState += " one more time and I'll seg fault"; break;
                                case 14: static bool JokeOver = false;
                                         if (!JokeOver)
                                         {
                                            JokeOver = true;
                                            printf ("\n%c[1;37;41m", ASCII_ESC);  // white on red
                                            printf ("Signal no. 11 received: Segmentation fault");
                                            printf ("%c[0m"      , ASCII_ESC);  // standard
                                            (void) QtUtilSleep (4000);
                                            printf ("     Just kidding  :-)");
                                         }
                                         StrState += " just kidding :-)";
                                default: break;
                             }
                          }
                          break;
      case AcquirePaused: StrState = t_MainWindow::tr("Device disconnected, acquisition paused" ); break;
      case VerifyPaused : StrState = t_MainWindow::tr("Device disconnected, verification paused"); break;
      case Cleanup      : StrState = t_MainWindow::tr("Cleanup" ); break;
      case Finished     : StrState = t_MainWindow::tr("Finished");
                          HasVerifications =  (pDevice->Acquisition1.VerifySrc) ||
                                              (pDevice->Acquisition1.VerifyDst) ||
                                              ((pDevice->Acquisition2.VerifyDst) && pDevice->Duplicate);
                          if (pDevice->Duplicate)
                          {
                             if (!pDevice->Error.Get() && !pDevice->Error.Get(1) && !pDevice->Error.Get(2) && pDevice->HashMatch())
                             {
                                if (HasVerifications)
                                     StrState += " - " + t_MainWindow::tr("Verified & ok", "Status column");
//                                else StrState += " - " + t_MainWindow::tr("ok"           , "Status column");
                             }
                             else
                             {
                                if (pDevice->Error.Get(1))
                                     StrState += " - " + t_MainWindow::tr("image 1 failed"); // message also appears if verification stops before end of image file
                                else if (!pDevice->HashMatch(&pDevice->Acquisition1))
                                     StrState += " - " + t_MainWindow::tr("verification 1 failed");
                                else StrState += " - " + t_MainWindow::tr("image 1 ok");

                                if (pDevice->Error.Get(2))
                                     StrState += " - " + t_MainWindow::tr("image 2 failed");
                                else if (!pDevice->HashMatch(&pDevice->Acquisition2))
                                     StrState += " - " + t_MainWindow::tr("verification 2 failed");
                                else StrState += " - " + t_MainWindow::tr("image 2 ok");

                                if (!pDevice->HashMatchSrc ())
                                   StrState += " - " + t_MainWindow::tr("source verification failed");
                             }
                          }
                          else
                          {
                             if (!pDevice->Error.Get() && !pDevice->Error.Get(1) && pDevice->HashMatch())
                             {
                                if (HasVerifications)
                                     StrState += " - " + t_MainWindow::tr("Verified & ok", "Status column");
//                                else StrState += " - " + t_MainWindow::tr("ok"           , "Status column");
                             }
                             else
                             {
                                if (pDevice->Error.Get(1))
                                     StrState += " - " + t_MainWindow::tr("image failed");
                                else if (!pDevice->HashMatch(&pDevice->Acquisition1))
                                     StrState += " - " + t_MainWindow::tr("verification failed");
//                                else StrState += "??";// should normally not happen, as common errors should lead to state "aborted" -> wrong, happens if image verification ok, source verification failed

                                if (!pDevice->HashMatchSrc ())
                                   StrState += " - " + t_MainWindow::tr("source verification failed");
                             }
                          }
                          break;
      case Aborted : StrState = t_MainWindow::tr("Aborted") + " - " + pDevice->GetErrorText ();
                     break;

      default      : CHK_EXIT (ERROR_DEVICE_BAD_STATE)
   }
   if (pDevice->State != Aborted)
   {
      QString Msg;
      CHK_EXIT (pDevice->GetMessage (Msg))
      if (!Msg.isEmpty())
         StrState += " - " + Msg;
   }

   return StrState;
}

QVariant t_Device::GetSectorSize (t_pDevice pDevice)
{
   return pDevice->SectorSize;
}

QVariant t_Device::GetSectorSizePhys (t_pDevice pDevice)
{
   return pDevice->SectorSizePhys;
}

QVariant t_Device::GetSize (t_pDevice pDevice)
{
   return pDevice->Size;
}

QVariant t_Device::GetSizeHumanFrac (t_pcDevice pDevice, bool SI, int FracLen, int UnitThreshold, bool OmitThousandsSeparator)
{
   QString Human;
   Human = UtilSizeToHuman (pDevice->Size, SI, FracLen, UnitThreshold);
   if (OmitThousandsSeparator)
      Human.replace (MainGetpNumberLocale()->groupSeparator(), "");
   return Human;
}

QVariant t_Device::GetSizeHuman (t_pDevice pDevice)
{
   return GetSizeHumanFrac (pDevice, true, 1);
}

static QString DeviceMediaInfoStr (t_MediaInfo::t_MediaState State)
{
   QString Str;
   switch (State)
   {
      case t_MediaInfo::Unknown: Str = t_MainWindow::tr("Unknown", "Media info string for informing about hidden areas (HPA/DCO)"); break;
      case t_MediaInfo::No     : Str = t_MainWindow::tr("No"     , "Media info string for informing about hidden areas (HPA/DCO)"); break;
      case t_MediaInfo::Yes    : Str = t_MainWindow::tr("Yes"    , "Media info string for informing about hidden areas (HPA/DCO)"); break;
      default: CHK_EXIT (ERROR_DEVICE_INVALID_MEDIAINFOSTATE)
   }
   return Str;
}

QVariant t_Device::GetHiddenAreas (t_pDevice pDevice)
{
   QString Str;

   if      ((pDevice->MediaInfo.HasHPA == t_MediaInfo::No     ) && (pDevice->MediaInfo.HasDCO == t_MediaInfo::No     )) Str = t_MainWindow::tr("none"   , "column hidden areas");
   else if ((pDevice->MediaInfo.HasHPA == t_MediaInfo::Unknown) && (pDevice->MediaInfo.HasDCO == t_MediaInfo::Unknown)) Str = t_MainWindow::tr("unknown", "column hidden areas");
   else
   {
      Str  = "HPA:"    + DeviceMediaInfoStr(pDevice->MediaInfo.HasHPA);
      Str += " / DCO:" + DeviceMediaInfoStr(pDevice->MediaInfo.HasDCO);
   }

   return Str;
}

QVariant t_Device::GetBadSectorCount (t_pDevice pDevice)
{
   if (!pDevice->StartTimestamp.isNull())
        return pDevice->GetBadSectorCount (false);
   else return "";
}

static void DeviceGetProgress (t_pDevice pDevice, quint64 *pCurrent=nullptr, quint64 *pTotal=nullptr)
{
   quint64 Current, Total;
   bool    HasVerifications;

   Total = pDevice->Size;
   HasVerifications =  (pDevice->Acquisition1.VerifySrc) ||
                       (pDevice->Acquisition1.VerifyDst) ||
                      ((pDevice->Acquisition2.VerifyDst) && pDevice->Duplicate);
   if (HasVerifications)
      Total *= 2;

   if (pDevice->StartTimestampVerify.isNull())
   {
      if (pDevice->Duplicate)   // If there's one with an error then do not consider it - except if
      {                         // both have an error, take the min. value of both in that case
         Current = ULONG_LONG_MAX;
         if (pDevice->Error.Get(1) == t_Device::t_Error::None)  Current = GETMIN (Current, pDevice->Acquisition1.CurrentWritePos.Get());
         if (pDevice->Error.Get(2) == t_Device::t_Error::None)  Current = GETMIN (Current, pDevice->Acquisition2.CurrentWritePos.Get());
         if (pDevice->Error.Get(1) && pDevice->Error.Get(2))    Current = GETMIN (pDevice->Acquisition1.CurrentWritePos.Get(),
                                                                                  pDevice->Acquisition2.CurrentWritePos.Get());
      }
      else
      {
         Current = pDevice->Acquisition1.CurrentWritePos.Get();
      }
   }
   else
   {
      Current = pDevice->GetCurrentVerifyPos() + pDevice->Size;
   }

   if (pTotal  ) *pTotal   = Total;
   if (pCurrent) *pCurrent = Current;
}

QVariant t_Device::GetProgress (t_pDevice pDevice)
{
   quint64 Current, Total;

   if (!pDevice->StartTimestamp.isNull())
   {
      DeviceGetProgress (pDevice, &Current, &Total);
      return (double) Current / Total;
   }

   return 0.0;
}

QVariant t_Device::GetProgressPercent (t_pDevice pDevice)
{
   quint64 Current, Total;

   if (!pDevice->StartTimestamp.isNull())
   {
      DeviceGetProgress (pDevice, &Current, &Total);
      return (int) ((100*Current) / Total);
   }

   return 0;
}

QVariant t_Device::GetCurrentSpeed (t_pDevice pDevice)
{
   QString Result;
   QTime   Now;
   double  MBs;
   double  Speed;
   int     MilliSeconds;
   quint64 CurrentPos;

   if ((pDevice->State == Acquire)||  // Don't display anything if no acquisition is running
       (pDevice->State == Launch) ||
       (pDevice->State == Verify))
   {
      Now = QTime::currentTime();
      DeviceGetProgress (pDevice, &CurrentPos);
      if (!pDevice->PrevTimestamp.isNull())
      {
         MilliSeconds = pDevice->PrevTimestamp.msecsTo (Now); // As this fn is called within 1s-interval, time_t would not be precise enough
         if (MilliSeconds > 0)
         {
            MBs   = (double) (CurrentPos - pDevice->PrevPos) / BYTES_PER_MEGABYTE;
            Speed = (1000.0*MBs) / MilliSeconds;
            pDevice->PrevSpeed = (pDevice->PrevSpeed + Speed) / 2.0;
         }
         Result = MainGetpNumberLocale()->toString (pDevice->PrevSpeed, 'f', 2);
      }
      pDevice->PrevTimestamp = Now;
      pDevice->PrevPos       = CurrentPos;
   }

   return Result;
}

QVariant t_Device::GetAverageSpeed (t_pDevice pDevice)
{
   QLocale Locale;
   QString Result;
   double  MBs;
   int     Seconds;
   quint64 CurrentPos;

   if (pDevice->StartTimestamp.isNull())
      return QString();

   if (pDevice->StopTimestamp.isNull()) // As long as the stop timestamp is Null, the acquisition is still running
        Seconds = pDevice->StartTimestamp.secsTo (QDateTime::currentDateTime());
   else Seconds = pDevice->StartTimestamp.secsTo (pDevice->StopTimestamp);

   if (Seconds == 0)
      return QString();

   if ((pDevice->StopTimestamp.isNull()) && (Seconds < DEVICE_MIN_RUNNING_SECONDS))
      return "--";

   DeviceGetProgress (pDevice, &CurrentPos);
   MBs = ((double) CurrentPos) / BYTES_PER_MEGABYTE;
   Result = Locale.toString (MBs/Seconds, 'f', 2);

   return Result;
}

QVariant t_Device::GetRemaining (t_pDevice pDevice)
{
   QString      Result;
   char         Buff[20];
   int          TotalSeconds;
   time_t       Now;
   unsigned int hh, mm, ss;
   quint64      Current, Total;

   if (!pDevice->Error.Abort() && ((pDevice->State == Acquire) || // Don't display anything if no acquisition is running
                                   (pDevice->State == Launch)  ||
                                   (pDevice->State == Verify)))
   {
      time (&Now);
      TotalSeconds = pDevice->StartTimestamp.secsTo (QDateTime::currentDateTime());
      if (TotalSeconds < DEVICE_MIN_RUNNING_SECONDS)
      {
         Result = "--";
      }
      else
      {
         DeviceGetProgress (pDevice, &Current, &Total);
         ss  = (unsigned int) ((double)Total / Current * TotalSeconds); // Estimated total time
         ss -= TotalSeconds;                                   // Estimated remaining time
         hh = ss / SECONDS_PER_HOUR;   ss %= SECONDS_PER_HOUR;
         mm = ss / SECONDS_PER_MINUTE; ss %= SECONDS_PER_MINUTE;
         snprintf (Buff, sizeof(Buff), "%02d:%02d:%02d", hh, mm, ss);
         Result = Buff;
      }
   }

   return Result;
}

static inline int DeviceFifoUsage (t_pFifo pFifo)
{
   int Usage;
   CHK_EXIT (pFifo->Usage(Usage))
   return Usage;
}

QVariant t_Device::GetFifoStatus (t_pDevice pDevice)
{
   QString Result;

   if ((pDevice->State == Acquire) ||  // Don't display anything if no acquisition is running
       (pDevice->State == Launch)  ||
       (pDevice->State == Verify))
   {
      int FifoUsageWrite = 0;                                    // The write thread gets a lot of dummy blocks into its Fifo (from SlotThreadCompressFinished), but
                                                                 // only 1 of them is taken out (then, the write thread already exits its loop). If image verification is on
      if (pDevice->State != Verify)                              // then these dummy blocks remain the in the write fifo, leading to an ugly fifo usage display on the
      {                                                          // GUI. As I do not like to add code for remove dummy blocks (and waiting until they all have been
         FifoUsageWrite = DeviceFifoUsage(pDevice->pFifoWrite1); // received etc.) I decided to zero them artificially here.
         if (pDevice->Duplicate)
            FifoUsageWrite = (FifoUsageWrite + DeviceFifoUsage(pDevice->pFifoWrite2)) / 2;
      }

      if (!pDevice->HasHashThread() && !pDevice->HasCompressionThreads())
      {
         Result = QString ("r %1 w") .arg (FifoUsageWrite);
      }
      else if (!pDevice->HasHashThread() && pDevice->HasCompressionThreads())
      {
         Result = QString ("r %1 c %2 w") .arg (DeviceFifoUsage(pDevice->pFifoRead      ))
                                          .arg (FifoUsageWrite);
      }
      else if (pDevice->HasHashThread() && !pDevice->HasCompressionThreads())
      {
         Result = QString ("r %1 m %2 w") .arg (DeviceFifoUsage(pDevice->pFifoRead   ))
                                          .arg (FifoUsageWrite);
      }
      else if (pDevice->HasHashThread() && pDevice->HasCompressionThreads())
      {
         Result = QString ("r %1 h %2 c %3 w") .arg (DeviceFifoUsage(pDevice->pFifoRead       ))
                                               .arg (DeviceFifoUsage(pDevice->pFifoHashOut    ))
                                               .arg (FifoUsageWrite);
      }
   }

   return Result;
}

//lint -restore

const char *t_Device::StateStr (void) const
{
   const char *pStr;
   switch (State)
   {
      case Idle         : pStr = "Idle";           break;
      case Queued       : pStr = "Queued";         break;
      case Launch       : pStr = "Launch";         break;
      case LaunchPaused : pStr = "LaunchPaused";   break;
      case Acquire      : pStr = "Acquire";        break;
      case AcquirePaused: pStr = "AcquirePaused";  break;
      case Verify       : pStr = "Verify";         break;
      case VerifyPaused : pStr = "VerifyPaused";   break;
      case Cleanup      : pStr = "Cleanup";        break;
      case Finished     : pStr = "Finished";       break;
      case Aborted      : pStr = "Aborted";        break;
      default           : pStr = "Unknown";
   }

   return pStr;
}

static void DeviceReplace (QString &Data, const QString &Token, const QString &Val, bool HasData=true, bool CaseSensitive=false)
{
   QString NewVal = Val;

   if (!HasData)
      NewVal = "--";
   Data.replace (Token, NewVal, CaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

static void DeviceReplace (QString &Data, const QString &Token, unsigned long long Val, bool HasData=true, bool CaseSensitive=false)
{
   DeviceReplace (Data, Token, QString::number(Val), HasData, CaseSensitive);
}

static void DeviceReplace (QString &Data, const QString &Token, bool Val, bool HasData=true, bool CaseSensitive=false)
{
   DeviceReplace (Data, Token, QString (Val ? "YES" : "NO"), HasData, CaseSensitive);
}

void t_Device::ProcessTokenString (QString &Str, t_pDevice pDevice)
{
   QString   Extension;
   QDateTime Now = QDateTime::currentDateTime();

   if (pDevice)
   {
      CHK_EXIT (t_File::GetFormatExtension (pDevice, nullptr, &Extension))

      DeviceReplace (Str, "%dev%"            , pDevice->LinuxDevice );
      DeviceReplace (Str, "%Size%"           , pDevice->Size        );
      DeviceReplace (Str, "%SizeHuman%"      , GetSizeHumanFrac (pDevice, true, 0, 10000, false).toString());
      DeviceReplace (Str, "%SizeHumanNoSep%" , GetSizeHumanFrac (pDevice, true, 0, 10000, true ).toString());
      DeviceReplace (Str, "%State%"          , QString(pDevice->StateStr()));
      DeviceReplace (Str, "%Serial%"         , pDevice->SerialNumber);
      DeviceReplace (Str, "%Model%"          , pDevice->Model       );
      DeviceReplace (Str, "%LocalDevice%"    , pDevice->Local       );

      DeviceReplace (Str, "%CaseNumber%"     , pDevice->Acquisition1.CaseNumber);
      DeviceReplace (Str, "%Examiner%"       , pDevice->Acquisition1.Examiner);
      DeviceReplace (Str, "%EvidenceNumber%" , pDevice->Acquisition1.EvidenceNumber);
      DeviceReplace (Str, "%Description%"    , pDevice->Acquisition1.Description);
      DeviceReplace (Str, "%Notes%"          , pDevice->Acquisition1.Notes);
      DeviceReplace (Str, "%Image%"          , pDevice->Acquisition1.ImagePath + pDevice->Acquisition1.ImageFilename + Extension);
      DeviceReplace (Str, "%InfoFile%"       , pDevice->Acquisition1.InfoPath  + pDevice->Acquisition1.InfoFilename  + t_File::pExtensionInfo);
      DeviceReplace (Str, "%SplitFileSize%"  , pDevice->Acquisition1.SplitFileSize);
      DeviceReplace (Str, "%VerifySrc%"      , pDevice->Acquisition1.VerifySrc);
      DeviceReplace (Str, "%VerifyDst%"      , pDevice->Acquisition1.VerifyDst);
      DeviceReplace (Str, "%CalcMD5%"        , pDevice->Acquisition1.CalcMD5);
      DeviceReplace (Str, "%CalcSHA1%"       , pDevice->Acquisition1.CalcSHA1);
      DeviceReplace (Str, "%CalcSHA256%"     , pDevice->Acquisition1.CalcSHA256);
      DeviceReplace (Str, "%Clone%"          , pDevice->Acquisition1.Clone);

      DeviceReplace (Str, "%Duplicate%"      , pDevice->Duplicate);
      DeviceReplace (Str, "%CaseNumber2%"    , pDevice->Acquisition2.CaseNumber    , pDevice->Duplicate);
      DeviceReplace (Str, "%Examiner2%"      , pDevice->Acquisition2.Examiner      , pDevice->Duplicate);
      DeviceReplace (Str, "%EvidenceNumber2%", pDevice->Acquisition2.EvidenceNumber, pDevice->Duplicate);
      DeviceReplace (Str, "%Description2%"   , pDevice->Acquisition2.Description   , pDevice->Duplicate);
      DeviceReplace (Str, "%Notes2%"         , pDevice->Acquisition2.Notes         , pDevice->Duplicate);
      DeviceReplace (Str, "%Image2%"         , pDevice->Acquisition2.ImagePath + pDevice->Acquisition2.ImageFilename + Extension             , pDevice->Duplicate);
      DeviceReplace (Str, "%InfoFile2%"      , pDevice->Acquisition2.InfoPath  + pDevice->Acquisition2.InfoFilename  + t_File::pExtensionInfo, pDevice->Duplicate);
      DeviceReplace (Str, "%VerifyDst2%"     , pDevice->Acquisition2.VerifyDst     , pDevice->Duplicate);

      DeviceReplace (Str, "%CurrentSpeed%"   , GetCurrentSpeed   (pDevice).toString());
      DeviceReplace (Str, "%AverageSpeed%"   , GetAverageSpeed   (pDevice).toString());
      DeviceReplace (Str, "%Progress%"       , GetProgressPercent(pDevice).toString());
      DeviceReplace (Str, "%TimeRemaining%"  , GetRemaining      (pDevice).toString());
      DeviceReplace (Str, "%BadSectors%"     , GetBadSectorCount (pDevice).toString());
      DeviceReplace (Str, "%HiddenAreas%"    , GetHiddenAreas    (pDevice).toString());
      DeviceReplace (Str, "%ExtendedState%"  , GetState          (pDevice).toString());
      DeviceReplace (Str, "%UserField%"      , GetUserField      (pDevice).toString());
      DeviceReplace (Str, "%AddStateInfo%"   , GetAddStateInfo   (pDevice).toString());
   }

   DeviceReplace (Str, "%Version%" , QString(pCompileInfoVersion));
   DeviceReplace (Str, "%MacAddr%" , CfgGetMacAddress ());
   DeviceReplace (Str, "%HostName%", CfgGetHostName   ());
   DeviceReplace (Str, "%d%"       , Now.toString("d"   ), true, true);   // Day of month  1-31
   DeviceReplace (Str, "%dd%"      , Now.toString("dd"  ), true, true);   //   "          01-31
   DeviceReplace (Str, "%ddd%"     , Now.toString("ddd" ), true, true);   //   "          Mon, Die, ...
   DeviceReplace (Str, "%dddd%"    , Now.toString("dddd"), true, true);   //   "          Montag, Dienstag, ...
   DeviceReplace (Str, "%M%"       , Now.toString("M"   ), true, true);   // Month         1-12
   DeviceReplace (Str, "%MM%"      , Now.toString("MM"  ), true, true);   //   "          01-12
   DeviceReplace (Str, "%MMM%"     , Now.toString("MMM" ), true, true);   //   "          Jan, Feb, ...
   DeviceReplace (Str, "%MMMM%"    , Now.toString("MMMM"), true, true);   //   "          Januar, Februar, ...
   DeviceReplace (Str, "%yy%"      , Now.toString("yy"  ), true, true);   // Year         13
   DeviceReplace (Str, "%yyyy%"    , Now.toString("yyyy"), true, true);   //   "          2013
   DeviceReplace (Str, "%h%"       , Now.toString("h"   ), true, true);   // Hour          0-23
   DeviceReplace (Str, "%hh%"      , Now.toString("hh"  ), true, true);   //   "          00-23
   DeviceReplace (Str, "%m%"       , Now.toString("m"   ), true, true);   // Minute        0-59
   DeviceReplace (Str, "%mm%"      , Now.toString("mm"  ), true, true);   //   "          00-59
   DeviceReplace (Str, "%s%"       , Now.toString("s"   ), true, true);   // Second        0-59
   DeviceReplace (Str, "%ss%"      , Now.toString("ss"  ), true, true);   //   "          00-59
   DeviceReplace (Str, "%AP%"      , Now.toString("AP"  ), true, true);   // AM / PM
   DeviceReplace (Str, "%ap%"      , Now.toString("ap"  ), true, true);   // am / pm
}

// ---------------------------
//        t_DeviceList
// ---------------------------

t_DeviceList::t_DeviceList(void)
   :QList<t_pDevice>()
{
   static bool Initialised = false;

   if (!Initialised)
   {
      Initialised = true;
      qRegisterMetaType<t_pDeviceList>("t_pDeviceList");
      qRegisterMetaType<t_pDevice>    ("t_pDevice"    );
   }
}

t_DeviceList::~t_DeviceList()
{
   int i;

   for (i=0; i<count(); i++)
      delete at(i);
}

t_pDevice t_DeviceList::AppendNew (const QString &SerialNumber, const PedDevice *pPedDev)
{
   t_pDevice pDev;

   pDev = new t_Device (SerialNumber, pPedDev);
   append (pDev);

   return pDev;
}

t_pDevice t_DeviceList::AppendNew (const QString &SerialNumber, const QString &LinuxDevice, const QString &Model,
                                   quint64 SectorSize, quint64 SectorSizePhys, quint64 Size)
{
   t_pDevice pDev;

   pDev = new t_Device (SerialNumber, LinuxDevice, Model, SectorSize, SectorSizePhys, Size);
   append (pDev);

   return pDev;
}

APIRET t_DeviceList::MatchDevice (t_pcDevice pDevCmp, t_pDevice &pDevMatch)
{
   bool Found = false;
   int  i;

   for (i=0; (i<count()) && !Found; i++)
   {
      pDevMatch = at(i);
      if (pDevCmp->SpecialDevice)  // Special device, we may only compare the LinuxDevice string
      {
         Found = (pDevMatch->LinuxDevice == pDevCmp->LinuxDevice);
      }
      else  // Not a special device, we can use normal matching
      {
// Lines below commented out, as somebody might change assignement of the devices starting with the names shown below after Guymager has done a device scan
         if (pDevMatch->LinuxDevice.startsWith("/dev/loop") ||  // These are devices generated internally by the loop device driver,
             pDevMatch->LinuxDevice.startsWith("/dev/dm-" ) ||  // LVM or DM. we can be sure that they have unique names, so let's
             pDevMatch->LinuxDevice.startsWith("/dev/mapper") || 
             pDevMatch->LinuxDevice.startsWith("/dev/md"  ))    // avoid all the problems with empty serial numbers and so on.
         {
            Found = (pDevMatch->LinuxDevice == pDevCmp->LinuxDevice);
         }
         else
              if (pDevMatch->SerialNumber.isEmpty())
         {
            Found = (pDevCmp->SerialNumber.isEmpty() && (pDevMatch->GetState() != t_Device::AcquirePaused) &&
                                                        (pDevMatch->GetState() != t_Device::VerifyPaused ) &&
                                                        (pDevMatch->Model      == pDevCmp->Model         ) &&
                                                        (pDevMatch->Size       == pDevCmp->Size          ));
         }
         else
         {
//         Found = (pDevMatch->SerialNumber == pDevCmp->SerialNumber);
//         if (Found)
//         {                                                                                            // If the serial numbers match, the whole
//            if (pDevMatch->Model != pDevCmp->Model) return ERROR_DEVICE_SERNR_MATCH_MODEL_MISMATCH;   // rest must match as well. Otherwise,
//            if (pDevMatch->Size  != pDevCmp->Size ) return ERROR_DEVICE_SERNR_MATCH_LENGTH_MISMATCH;  // something very strange is going on...
//         }

// The comparision above cannot be used, because there are certain devices, that behave as if 2 devices are connected when
// plugged in - with the same serial number!! Example: I once had a memory stick from Intuix that reported 2 devices: The
// memory stick itself and a CD-Rom containing device drivers for some strange OS. Both devices had the same serial number.
// That's why the check now also includes the size and the model.

            Found = ((pDevMatch->SerialNumber == pDevCmp->SerialNumber) &&
                     (pDevMatch->Size         == pDevCmp->Size )        &&
                     (pDevMatch->Model        == pDevCmp->Model));
         }
      }
   }

   if (!Found)
      pDevMatch = nullptr;

   return NO_ERROR;
}

bool t_DeviceList::UsedAsCloneDestination (t_pcDevice pDevChk)
{
   t_pcDevice pDevCmp;
   int         i;

   for (i=0; i<count(); i++)
   {
      pDevCmp = at(i);
      if ( (pDevCmp->GetState() != t_Device::Idle    ) &&
           (pDevCmp->GetState() != t_Device::Finished) &&
           (pDevCmp->GetState() != t_Device::Aborted ) &&
           (pDevCmp->Acquisition1.Clone          ))
      {
         if ((pDevCmp->Acquisition1.ImagePath + pDevCmp->Acquisition1.ImageFilename) == pDevChk->LinuxDevice)
            return true;

         if (  pDevCmp->Duplicate &&
             ((pDevCmp->Acquisition2.ImagePath + pDevCmp->Acquisition2.ImageFilename) == pDevChk->LinuxDevice))
            return true;
      }
   }
   return false;
}

QString t_DeviceList::GetOverallAcquisitionSpeed (void)
{
   QString Result;
   double  Sum   = 0.0;
   int     Count = 0;

   t_pcDevice pDevice;
   int         i;

   for (i=0; i<count(); i++)
   {
      pDevice = at(i);
      if ((pDevice->GetState() == t_Device::Acquire) ||
          (pDevice->GetState() == t_Device::Launch)  ||
          (pDevice->GetState() == t_Device::Verify))
      {
         Count++;
         Sum  += pDevice->PrevSpeed;
      }
      if (Count)
           Result = MainGetpNumberLocale()->toString (Sum, 'f', 2);
      else Result = QString::null;
   }
   return Result;
}
