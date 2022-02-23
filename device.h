// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
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

#ifndef DEVICE_H
#define DEVICE_H

#include "hash.h"

#include <parted/parted.h>
#include <QMetaType>
#include <QReadWriteLock>
#include <QMutex>
#include <QTime>

#include "fifo.h"
#include "info.h"
#include "file.h"
#include "media.h"
#include "config.h"

class t_ThreadRead;
class t_ThreadHash;
class t_ThreadCompress;
class t_ThreadWrite;

#define DEVICE_DEFAULT_SECTOR_SIZE ((size_t)512)

class t_DevicePos
{
   public:
      t_DevicePos () :
          oPos(0),
          oSem()
      {
      }

      quint64 Get (void)
      {
         oSem.lockForRead ();
         quint64 Pos = oPos;
         oSem.unlock ();
         return Pos;
      }

      void Set (quint64 Pos)
      {
         oSem.lockForWrite ();
         oPos = Pos;
         oSem.unlock ();
      }

      void Zero (void)
      {
         Set (0LL);
      }

      void Inc (quint64 Ofs)
      {
         oSem.lockForWrite ();
         oPos += Ofs;
         oSem.unlock ();
      }

   private:
      quint64        oPos;
      QReadWriteLock oSem;
};

class t_ImageFileHash // Holds the hash of a complete image file (not just the image contents)
{
   public:
      t_ImageFileHash (const QString &Filename, t_pHashMD5Digest pMD5Digest=NULL, bool MD5ok=false)
      {
         this->Filename = Filename;
         MD5Valid = (pMD5Digest != NULL) && MD5ok;
         if (MD5Valid)
            MD5Digest = *pMD5Digest;
      }

   public:
      QString         Filename;
      bool            MD5Valid;
      t_HashMD5Digest MD5Digest;
};

typedef QList<t_ImageFileHash *> t_ImageFileHashList;

class t_Acquisition
{
   public:
      t_Acquisition (t_pDevice pDev, int Inst=-1) :
         Info     (this),
         pDevice  (pDev),
         Instance (Inst)
      {
         Format        = t_File::NotSet;
         Clone         = false;
         CalcMD5       = false;
         CalcSHA1      = false;
         CalcSHA256    = false;
         VerifySrc     = false;
         VerifyDst     = false;
         SplitFileSize = 0;
         memset (&MD5DigestVerifyDst   , 0, sizeof(t_HashMD5Digest   ));
         memset (&SHA1DigestVerifyDst  , 0, sizeof(t_HashSHA1Digest  ));
         memset (&SHA256DigestVerifyDst, 0, sizeof(t_HashSHA256Digest));
      };

      void SetDialogValues (const t_Acquisition &In)
      {
         ImagePath      = In.ImagePath;
         InfoPath       = In.InfoPath;
         ImageFilename  = In.ImageFilename;
         InfoFilename   = In.InfoFilename;
         Format         = In.Format;
         Clone          = In.Clone;
         CalcMD5        = In.CalcMD5;
         CalcSHA1       = In.CalcSHA1;
         CalcSHA256     = In.CalcSHA256;
         VerifySrc      = In.VerifySrc;
         VerifyDst      = In.VerifyDst;
         CaseNumber     = In.CaseNumber;
         EvidenceNumber = In.EvidenceNumber;
         Examiner       = In.Examiner;
         Description    = In.Description;
         Notes          = In.Notes;
         UserField      = In.UserField;
         SplitFileSize  = In.SplitFileSize;
      }

   public: // Settings from             Same value in ac-   Remarks
           // the user dialog           quisition 1 and 2
           // -------------------------------------------------------------------------------
      QString        ImagePath;      //                     Always end with /
      QString        InfoPath;       //                     Always end with /
      QString        ImageFilename;  //                     Image filename, without extension
      QString        InfoFilename;   //                     Info filename, without extension
      t_File::Format Format;         //       x
      bool           Clone;          //       x
      bool           CalcMD5;        //       x
      bool           CalcSHA1;       //       x
      bool           CalcSHA256;     //       x
      bool           VerifySrc;      //       x
      bool           VerifyDst;      //
      QString        CaseNumber;     //
      QString        EvidenceNumber; //
      QString        Examiner;       //
      QString        Description;    //
      QString        Notes;          //
      QString        UserField;      //
      quint64        SplitFileSize;  //       ?              See t_File::GetFormatExtension; 0 means do not split

      // Variables used during acquisition
      t_Info                    Info;                // Must only be used if InfoFilename is valid!
      t_DevicePos               CurrentWritePos;
      t_DevicePos               CurrentVerifyPosDst;
      t_HashMD5Digest           MD5DigestVerifyDst;
      t_HashSHA1Digest          SHA1DigestVerifyDst;
      t_HashSHA256Digest        SHA256DigestVerifyDst;
      t_ImageFileHashList       ImageFileHashList;

      // Other
      t_pDevice pDevice;
      int        Instance;
};

class t_Device
{
   public:

      typedef enum
      {
         Idle,
         Queued,
         Launch,          // The two Launch states have been introduced for handling the situation where device is replaced by another one and user starts
         LaunchPaused,    // acquistion without device rescan. State moves to from Launch to Acquire on first rescan if the device is found and its path is
         Acquire,         // still the same. LaunchPaused already is an error (detected by read thread). It happens if read thread detects the error before
         AcquirePaused,   // the rescan. Anyway, the final cleanup is always done in t_MainWindow::SlotScanFinished.
         Verify,
         VerifyPaused,
         Cleanup,
         Finished,
         Aborted
      } t_State;

      class t_Error
      {
         public:
            enum
            {
               // Common errors
               None = 0,
               UserRequest,
               AcquisitionStartFailed,
               DeviceMovedOnLaunch,
               ReadError,
               TooManyBadSectors,
               DisconnectTimeout,

               CommonMax,

               // ThreadWrite errors
               WriteError,
               VerifyError,
               InfoError
            };

         public:
            t_Error (t_pcDevice pDevice);

            void    Set     (uint Err, int WriteInstance=0);
            uint    Get     (int WriteInstance=0) const;
            QString GetText (void) const;
            bool    Abort   (int WriteInstance=0) const;
            void    Clear   (void);

         private:
            t_pcDevice poDevice;
            uint        oErrorCommon;
            uint        oErrorWrite1;
            uint        oErrorWrite2;
            bool        oAbort;
      };


      static const int FileDescEmpty     = -1;

      class t_AddStateInfo       // Allows the user to add state information to a device
      {
          public:
             t_AddStateInfo () : CanBeAcquired(true),
                                 Color(COLOR_DEFAULT)
             {};

          public:
             QString Info;
             bool    CanBeAcquired;
             int     Color;         // Refers to the color AdditionalState1, AdditionalState2, ... in the configuration file.
      };

   public:
      t_Device (const QString &SerialNumber=QString(), const PedDevice *pPedDev=NULL);
      t_Device (const QString &SerialNumber, const QString &LinuxDevice, const QString &Model,
                                             quint64 SectorSize, quint64 SectorSizePhys, quint64 Size=0,
                                             const QString &NativePath = QString(),
                                             const QString &ByPath     = QString(),
                                             const QString &Interface  = QString());
     ~t_Device ();

      static QVariant GetSerialNumber        (t_pDevice pDevice);
      static QVariant GetLinuxDevice         (t_pDevice pDevice);
      static QVariant GetNativePath          (t_pDevice pDevice);
      static QVariant GetByPath              (t_pDevice pDevice);
      static QVariant GetInterface           (t_pDevice pDevice);
      static QVariant GetModel               (t_pDevice pDevice);
      static QVariant GetState               (t_pDevice pDevice);
      static QVariant GetAddStateInfo        (t_pDevice pDevice);
      static QVariant GetSectorSize          (t_pDevice pDevice);
      static QVariant GetSectorSizePhys      (t_pDevice pDevice);
      static QVariant GetBadSectorCount      (t_pDevice pDevice);
      static QVariant GetBadSectorCountVerify(t_pDevice pDevice);
      static QVariant GetSize                (t_pDevice pDevice);
      static QVariant GetSizeHuman           (t_pDevice pDevice);
      static QVariant GetSizeHumanFrac       (t_pcDevice pDevice, bool SI=true, int FracLen=-1, int UnitThreshold=-1, bool OmitThousandsSeparator=false);
      static QVariant GetHiddenAreas         (t_pDevice pDevice);
      static QVariant GetProgress            (t_pDevice pDevice);
      static QVariant GetProgressPercent     (t_pDevice pDevice);
      static QVariant GetCurrentSpeed        (t_pDevice pDevice);
      static QVariant GetAverageSpeed        (t_pDevice pDevice);
      static QVariant GetRemaining           (t_pDevice pDevice);
      static QVariant GetFifoStatus          (t_pDevice pDevice);
      static QVariant GetUserField           (t_pDevice pDevice);
      static QVariant GetExaminer            (t_pDevice pDevice);

      inline quint64 GetCurrentVerifyPos (void)
      {
         quint64 Pos = ULONG_LONG_MAX;

         if (Acquisition1.VerifySrc)              Pos = GETMIN (Pos, CurrentVerifyPosSrc.Get());
         if (Duplicate)
         {
            if (Acquisition1.VerifyDst && (Error.Get(1) == t_Error::None)) Pos = GETMIN (Pos, Acquisition1.CurrentVerifyPosDst.Get());
            if (Acquisition2.VerifyDst && (Error.Get(2) == t_Error::None)) Pos = GETMIN (Pos, Acquisition2.CurrentVerifyPosDst.Get());
         }
         else
         {
            if (Acquisition1.VerifyDst) Pos = GETMIN (Pos, Acquisition1.CurrentVerifyPosDst.Get());
         }

         if (Pos == ULONG_LONG_MAX)
            Pos = 0;

         return Pos;
      }

      inline void AddBadSector (quint64 Sector)
      {
         SemBadSectors.lock ();
         if (State == Acquire)
              BadSectors      .append(Sector);
         else BadSectorsVerify.append(Sector);
         SemBadSectors.unlock ();
      }

      APIRET GetBadSectors (QList<quint64> &BadSectorsCopy, bool GetVerify)
      {
         SemBadSectors.lock ();
         if (GetVerify)
              BadSectorsCopy = BadSectorsVerify;
         else BadSectorsCopy = BadSectors;
         SemBadSectors.unlock ();
         return NO_ERROR;
      }

      quint64 GetBadSectorCount (bool GetVerify)
      {
         quint64 Count;
         SemBadSectors.lock ();
         if (GetVerify)
              Count = BadSectorsVerify.count();  //lint !e732 Loss of sign
         else Count = BadSectors      .count();  //lint !e732 Loss of sign
         SemBadSectors.unlock ();
         return Count;
      }

      APIRET ClearBadSectors (void)
      {
         SemBadSectors.lock ();
         BadSectorsVerify.clear();
         BadSectors      .clear();
         SemBadSectors.unlock ();
         return NO_ERROR;
      }

      APIRET GetMessage (QString &MessageCopy)
      {
         SemMessage.lock ();
         MessageCopy = Message;
         SemMessage.unlock ();
         return NO_ERROR;
      }

      APIRET SetMessage (const QString &NewMessage)
      {
         SemMessage.lock ();
         Message = NewMessage;
         SemMessage.unlock ();
         return NO_ERROR;
      }

      APIRET WakeWaitingThreads()
      {
         if      (!HasHashThread() && !HasCompressionThreads())
         {
            CHK (pFifoRead->WakeWaitingThreads())
         }
         else if ( HasHashThread() && !HasCompressionThreads())
         {
            CHK (pFifoRead   ->WakeWaitingThreads())
            CHK (pFifoHashOut->WakeWaitingThreads())
         }
         else if (!HasHashThread() &&  HasCompressionThreads())
         {
            CHK (pFifoCompressIn ->WakeWaitingThreads())
            CHK (pFifoCompressOut->WakeWaitingThreads())
         }
         else if ( HasHashThread() &&  HasCompressionThreads())
         {
            CHK (pFifoRead       ->WakeWaitingThreads())
            CHK (pFifoCompressIn ->WakeWaitingThreads())
            CHK (pFifoCompressOut->WakeWaitingThreads())
         }
         return NO_ERROR;
      }

      bool HashMatchSrc (void) const
      {
         if (Acquisition1.VerifySrc)
         {
            if (!HashMD5Match   (&MD5Digest   , &MD5DigestVerifySrc   )) return false;
            if (!HashSHA1Match  (&SHA1Digest  , &SHA1DigestVerifySrc  )) return false;
            if (!HashSHA256Match(&SHA256Digest, &SHA256DigestVerifySrc)) return false;
         }
         return true;
      }

      bool HashMatch (t_pcAcquisition pAcquisition) const
      {
         if (pAcquisition->VerifyDst && (pAcquisition->pDevice->Error.Get(pAcquisition->Instance) == t_Error::None))
         {
            if (!HashMD5Match   (&MD5Digest   , &(pAcquisition->MD5DigestVerifyDst   ))) return false;
            if (!HashSHA1Match  (&SHA1Digest  , &(pAcquisition->SHA1DigestVerifyDst  ))) return false;
            if (!HashSHA256Match(&SHA256Digest, &(pAcquisition->SHA256DigestVerifyDst))) return false;
         }
         return true;
      }


      bool HashMatch (void) const // return true only if all hashes match
      {
         if (!HashMatchSrc())            return false;
         if (!HashMatch(&Acquisition1))  return false;
         if (Duplicate)
            if (!HashMatch(&Acquisition2))
               return false;
         return true;
      }

      inline t_State GetState (void) const
      {
         return State;
      }

      void SetState (t_State NewState)
      {
         State = NewState;
         if ((State == AcquirePaused) || (State == VerifyPaused))
            DisconnectTime = time(NULL);
      }

      bool HasHashThread           (void) const;
      bool HasCompressionThreads   (void) const;
      bool HasActiveAcquistion     (void) const;
      bool DisconnectTimeoutReached(void) const;
      const char *StateStr         (void) const;

      QString GetErrorText         (void) const;

      static void ProcessTokenString (QString &Str, t_pDevice pDevice = NULL);

   private:
      void Initialise (void);
      void InitialiseDeviceSpecific (const QString &SerialNumber, const QString &LinuxDevice, const QString &Model,
                                    quint64 SectorSize, quint64 SectorSizePhys, quint64 Size,
                                    const QString &NativePath, const QString &ByPath, const QString &Interface);

   public:  QString                   SerialNumber;
            QString                   Model;
            QString                   LinuxDevice;      // for instance /dev/hda
            QString                   NativePath;       // for instance /sys/devices/pci0000:00/0000:00:1f.2/ata8/host7/target7:0:0/7:0:0:0/block/sdd
            QString                   ByPath;           // for instance /dev/disk/by-path/pci-0000:00:1f.2-scsi-0:0:0:0
            QString                   Interface;        // for instance usb, ata, virtual
            bool                      Local;            // it's a local device (cannot be acquired)
            quint64                   SectorSize;
            quint64                   SectorSizePhys;
            quint64                   Size;
            bool                      SpecialDevice;    // Special device that has been added manually to the device table
            bool                      Removable;
            t_MediaInfo               MediaInfo;

   private: t_State                   State;
            time_t                    DisconnectTime;   // If state is disconnected, this timestamp holds the moment when it happened
            QString                   Message;          // Used by the threads to communicate messages displayed in spreadsheet field "state"
   public:  t_AddStateInfo            AddStateInfo;
            t_Error                   Error;
            int                       AbortCount;       // Little easter egg  ;-)
            bool                      DeleteAfterAbort;

            bool                      Duplicate;        // True if image is written to 2 images files (i.e., is duplicated)
            t_Acquisition             Acquisition1;
            t_Acquisition             Acquisition2;     // Only in use of Duplicate is set

            int                       FileDescSrc;
            t_DevicePos               CurrentReadPos;
            t_DevicePos               CurrentVerifyPosSrc;
   private: QList<quint64>            BadSectors;          // Accessed concurrently, use appropriate functions
   private: QList<quint64>            BadSectorsVerify;    // Accessed concurrently, use appropriate functions
   public:  bool                      FallbackMode;        // Set if an error occurs while reading a large block. If set, sectors are read individually until the next large block boundary.
            QDateTime                 StartTimestamp;
            QDateTime                 StartTimestampVerify;
            QDateTime                 StopTimestamp;

   public:  t_ThreadRead            *pThreadRead;
            t_ThreadHash            *pThreadHash;
            t_ThreadWrite           *pThreadWrite1;
            t_ThreadWrite           *pThreadWrite2;
            QList<t_ThreadCompress *> ThreadCompressList;

   public:  t_pFifoMemory            pFifoMemory;
            t_pFifo                  pFifoRead;         // Pointers to the Fifos used by the different
            t_pFifo                  pFifoHashIn;       // threads. Some of them point to the same Fifos,
            t_pFifo                  pFifoHashOut;      // for instance pFifoRead and pFifoHashIn.
            t_pFifo                  pFifoWrite1;
            t_pFifo                  pFifoWrite2;
            t_pFifoCompressIn        pFifoCompressIn;
            t_pFifoCompressOut       pFifoCompressOut;
            int                       FifoMaxBlocks;
            unsigned int              FifoBlockSize;       // The pure size as it would be required by for the data
            unsigned int              FifoAllocBlockSize;  // The block size as it is used for real allocation

            t_HashMD5Digest           MD5Digest;
            t_HashMD5Digest           MD5DigestVerifySrc;
            t_HashSHA1Digest          SHA1Digest;
            t_HashSHA1Digest          SHA1DigestVerifySrc;
            t_HashSHA256Digest        SHA256Digest;
            t_HashSHA256Digest        SHA256DigestVerifySrc;

   public:  quint64                   PrevPos;          // Some variables for
            QTime                     PrevTimestamp;    // the current speed
            double                    PrevSpeed;        // calculation.
            bool                      Checked;          // Helper variables for matching algorithm ...
            bool                      AddedNow;         // ... in SlotScanFinished, not used elsewhere.

   private: QMutex                    SemBadSectors;
   private: QMutex                    SemMessage;
};

class t_DeviceList: public QList<t_pDevice>
{
   public:
      t_DeviceList (void);
     ~t_DeviceList ();

      t_pDevice AppendNew (const QString &SerialNumber, const PedDevice *pPedDev);
      t_pDevice AppendNew (const QString &SerialNumber, const QString &LinuxDevice, const QString &Model,
                           quint64 SectorSize, quint64 SectorSizePhys, quint64 Size=0);

      APIRET  MatchDevice                (t_pcDevice pDevCmp, t_pDevice &pDevMatch);
      bool    UsedAsCloneDestination     (t_pcDevice pDevChk);
      QString GetOverallAcquisitionSpeed (void);

};

typedef t_DeviceList *t_pDeviceList;

Q_DECLARE_METATYPE(t_pDeviceList); //lint !e19 Useless declaration


// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_DEVICE_SERNR_MATCH_MODEL_MISMATCH = ERROR_BASE_DEVICE + 1,
   ERROR_DEVICE_SERNR_MATCH_LENGTH_MISMATCH,
   ERROR_DEVICE_BAD_STATE,
   ERROR_DEVICE_BAD_ABORTREASON,
   ERROR_DEVICE_NOT_CLEAN,
   ERROR_DEVICE_INVALID_MEDIAINFOSTATE,
   ERROR_DEVICE_INVALID_ERROR
};


#endif

