// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for writing data.
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
#include "compileinfo.h"

#include <errno.h>
#include <zlib.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include <QtCore>

#include "toolconstants.h"
#include "toolsysinfo.h"

#include "util.h"
#include "config.h"
#include "device.h"
#include "main.h"
#include "qtutil.h"
#include "threadwrite.h"


const unsigned long THREADWRITE_WAIT_FOR_HANDLE_GRANULARITY =   500; // ms
const unsigned long THREADWRITE_WAIT_FOR_HANDLE             = 30000; // ms
const unsigned long THREADWRITE_SLOWDOWN_SLEEP              =   700; // ms

const unsigned long THREADWRITE_ENCASE_MAXLEN_IMAGERVERSION = 11;
const unsigned long THREADWRITE_ENCASE_MAXLEN_OSVERSION     = 23;


class t_OutputFile
{
   public:
                      t_OutputFile  (t_ThreadWrite *pThreadWrite) :poThreadWrite(pThreadWrite) {}
      virtual        ~t_OutputFile  () {}
      virtual APIRET  Open          (t_pAcquisition    pAcquisition, bool Verify) = 0;
      virtual APIRET  Write         (t_pFifoBlock      pFifoBlock)   = 0;
      virtual APIRET  Verify        (t_pHashContextMD5 pHashContextMD5, t_pHashContextSHA1 pHashContextSHA1, t_pHashContextSHA256 pHashContextSHA256, t_ImageFileHashList &ImageFileHashList, quint64 *pPos) = 0;
      virtual APIRET  Close         (void)                           = 0;
      virtual bool    Opened        (void)                           = 0;
      virtual void *  GetFileHandle (void)                           = 0;

      t_ThreadWrite *poThreadWrite;
};

// ------------------------------------------------------------------------------------------------
//                                            DD FORMAT
// ------------------------------------------------------------------------------------------------

class t_OutputFileDD: public t_OutputFile
{
   public:
      t_OutputFileDD (t_ThreadWrite *pThreadWrite) :
          t_OutputFile         (pThreadWrite),
          oFile                (-1),
          oLastCheckT          (0),
         poVerifyBuff          (nullptr),
         poAcquisition         (nullptr),
         poDevice              (nullptr),
          oSplitNr             (0),
          oCurrentVerifyFileNr (0)
      {
      } //lint -esym(613, t_OutputFileDD::poFile)  Possible use of nullptr
        //lint -esym(668, fclose, fwrite)  Possibly passing nullptr

      ~t_OutputFileDD (void)
      {
         if (t_OutputFileDD::Opened())
            (void) t_OutputFileDD::Close();
      } //lint !e1740

      APIRET Open (t_pAcquisition pAcquisition, bool Verify)
      {
         QString Extension;

         poAcquisition = pAcquisition;
         poDevice      = pAcquisition->pDevice;
         if (CONFIG (WriteToDevNull))
         {
            oFilename = "/dev/null";
         }
         else
         {
            CHK (t_File::GetFormatExtension (poDevice, &Extension))
            oFilename = pAcquisition->ImagePath + pAcquisition->ImageFilename;
            if (pAcquisition->SplitFileSize == 0)
                 oFilename += Extension;
            else oSplitNrDecimals = t_File::GetDdSplitNrDecimals (poDevice->Size, pAcquisition->SplitFileSize);
         }

//       oFileFlags = O_NOATIME | Verify ? O_RDONLY : O_WRONLY; // Doesn't work... !??
         oFileFlags = O_NOATIME | O_RDWR;
         if (!pAcquisition->Clone && !Verify)
            oFileFlags |= O_CREAT;              // Create it if doesn't exist

         time (&oLastCheckT);

         return NO_ERROR;
      }

      APIRET Write (t_pFifoBlock pFifoBlock)
      {
         size_t   RemainingData = pFifoBlock->DataSize;
         size_t   Offset = 0;
         ssize_t  Written;
         time_t   NowT;

         while (RemainingData)
         {
            if (!Opened())
            {
               QString Extension;
               QString Filename;
               if (poAcquisition->SplitFileSize)
               {
                  Extension = QString(".%1").arg(oSplitNr++, oSplitNrDecimals, 10, QChar('0'));
                  oRemainingFileSize = poAcquisition->SplitFileSize;
               }
               else
               {
                  oRemainingFileSize = ULONG_LONG_MAX;
               }
               Filename = QSTR_TO_PSZ (oFilename+Extension); // Do a QSTR_TO_PSZ conversion here in order to work everywhere with exactly the same filename
               oFilenameList += Filename;
               oFile = open64 (QSTR_TO_PSZ (Filename), oFileFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
               if (oFile < 0)
               {
                  oFileFlags &= ~O_NOATIME;
                  oFile = open64 (QSTR_TO_PSZ (Filename), oFileFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
               }
               LOG_INFO ("[%s] Flag NOATIME on image destination switched %s", QSTR_TO_PSZ(poDevice->LinuxDevice), (oFileFlags & O_NOATIME) ? "on":"off")
               if (oFile < 0)
               {
                  LOG_ERROR ("[%s] open64 on %s failed, errno=%d '%s'", QSTR_TO_PSZ (poDevice->LinuxDevice), QSTR_TO_PSZ (oFilename), errno, ToolErrorTranslateErrno (errno))
                  return ERROR_THREADWRITE_OPEN_FAILED;
               }
            }

            Written = write (oFile, &pFifoBlock->Buffer[Offset], GETMIN(RemainingData, oRemainingFileSize));
            if (Written < 0)
            {
               LOG_ERROR ("[%s] Write failed, oFile %d, errno=%d '%s'", QSTR_TO_PSZ (poDevice->LinuxDevice), oFile, errno, ToolErrorTranslateErrno (errno))
               return ERROR_THREADWRITE_WRITE_FAILED;
            }
            RemainingData      -= Written;
            oRemainingFileSize -= Written;
            Offset             += Written;
            if (oRemainingFileSize == 0)
               Close();
         }

         if (poAcquisition->Clone)    // It was observed that the write or fwrite functions do not detect device removal! It is unknown
         {                                   // how this is possible... Therefore, we check the existence of the device every 2 seconds by means
            time (&NowT);                    // of a seperate fopen/flclose.
            if ((NowT - oLastCheckT) >=2)
            {
               FILE *pFile;
               bool   Error;

               pFile = fopen64 (QSTR_TO_PSZ (oFilename), "r");
               Error = (pFile == nullptr);
               if (!Error)
                  Error = (fclose (pFile) != 0);
               if (Error)
               {
                  LOG_ERROR ("[%s] Check for destination clone %s failed (%d '%s')", QSTR_TO_PSZ (poDevice->LinuxDevice), QSTR_TO_PSZ (oFilename), errno, ToolErrorTranslateErrno (errno))
                  return ERROR_THREADWRITE_WRITE_FAILED;
               }
               oLastCheckT = NowT;
            }
         }

         return NO_ERROR;
      }

      APIRET Verify (t_pHashContextMD5 pHashContextMD5, t_pHashContextSHA1 pHashContextSHA1, t_pHashContextSHA256 pHashContextSHA256, t_ImageFileHashList &ImageFileHashList, quint64 *pPos)
      {
         t_HashMD5Digest MD5Digest;
         ssize_t         Read;

         if (*pPos == 0)       // First call of this function?
         {
            poVerifyBuff = UTIL_MEM_ALLOC (poDevice->FifoBlockSize);
            if (poVerifyBuff == nullptr)
               CHK (ERROR_THREADWRITE_MALLOC_FAILED);
         }

         if (!Opened())
         {
            QString Filename = oFilenameList[oCurrentVerifyFileNr++];
            oFile = open64 (QSTR_TO_PSZ (Filename), oFileFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            if (oFile < 0)
            {
               oFileFlags &= ~O_NOATIME;
               oFile = open64 (QSTR_TO_PSZ (Filename), oFileFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            }
            off64_t FileSize=0;
            bool    Err = (oFile < 0);
            if (!Err) Err = ((FileSize = lseek64 (oFile,0,SEEK_END)) == -1);
            if (!Err) Err = (lseek64 (oFile, 0, SEEK_SET) != 0);
            if (Err)
            {
               LOG_ERROR ("[%s] Opening %s failed, errno=%d '%s'", QSTR_TO_PSZ (poDevice->LinuxDevice), QSTR_TO_PSZ (oFilename), errno, ToolErrorTranslateErrno (errno))
               return ERROR_THREADWRITE_VERIFY_FAILED;
            }
            if (poAcquisition->Clone)     // The file size query from above would not need to be done in that
               FileSize = poDevice->Size; // case, however we do it anyway as it is a small access test as well.

            oRemainingFileSize = (t_uint64) FileSize;
            if (CONFIG(CalcImageFileMD5))
               CHK (HashMD5Init (&oCurrentHashContextMD5))
         }

         Read = read (oFile, (char*)poVerifyBuff, GETMIN(poDevice->FifoBlockSize, oRemainingFileSize));
         if (Read < 0)
         {
            QString Filename = oFilenameList[oCurrentVerifyFileNr-1];
            LOG_ERROR ("[%s] Read failed, oFile %d [%s], seek pos %llu, errno %d '%s'", QSTR_TO_PSZ (poDevice->LinuxDevice), oFile, QSTR_TO_PSZ (Filename), *pPos, errno, ToolErrorTranslateErrno (errno))
            return ERROR_THREADWRITE_VERIFY_FAILED;
         }
         oRemainingFileSize -= Read;
         if (CONFIG(CalcImageFileMD5))
            CHK_EXIT (HashMD5Append (&oCurrentHashContextMD5, poVerifyBuff, Read))

         if (oRemainingFileSize == 0)
         {
            bool MD5Valid = false;
            if (CONFIG(CalcImageFileMD5))
            {
               CHK (HashMD5Digest (&oCurrentHashContextMD5, &MD5Digest))
               MD5Valid = true;
            }
            ImageFileHashList.append (new t_ImageFileHash (oFilenameList[oCurrentVerifyFileNr-1], &MD5Digest, MD5Valid));
            Close0 (false);
         }

         if (pHashContextMD5)    CHK_EXIT (HashMD5Append    (pHashContextMD5   , poVerifyBuff, Read))
         if (pHashContextSHA1)   CHK_EXIT (HashSHA1Append   (pHashContextSHA1  , poVerifyBuff, Read))
         if (pHashContextSHA256) CHK_EXIT (HashSHA256Append (pHashContextSHA256, poVerifyBuff, Read))

         *pPos += Read;

         return NO_ERROR;
      }

      APIRET Close0 (bool ReleaseMem)
      {
         int Res;

         if (oFile == -1)
            CHK (ERROR_THREADWRITE_NOT_OPENED)

         Res = close (oFile);
         oFile = -1;
         if (Res != 0)
         {
            LOG_ERROR ("[%s] Close failed, errno=%d '%s'", QSTR_TO_PSZ (poDevice->LinuxDevice), errno, ToolErrorTranslateErrno (errno))
            return ERROR_THREADWRITE_CLOSE_FAILED;
         }

         if (poVerifyBuff && ReleaseMem)
         {
            UTIL_MEM_FREE (poVerifyBuff);
            poVerifyBuff = nullptr;
         }
         return NO_ERROR;
      }

      APIRET Close (void)
      {
         return Close0 (true);
      }

      void * GetFileHandle (void)
      {
         if (oFile == -1)
              return nullptr;
         else return &oFile;
      }

      inline bool Opened (void)
      {
         return (oFile != -1);
      }

   private:
      QStringList       oFilenameList;
      int               oFile;
      time_t            oLastCheckT; // Only used when cloning, see above
      QString           oFilename;
      void            *poVerifyBuff;
      t_pAcquisition   poAcquisition;
      t_pDevice        poDevice;
      int               oSplitNr;
      int               oSplitNrDecimals;
      int               oFileFlags;
      t_uint64          oRemainingFileSize;
      int               oCurrentVerifyFileNr;
      t_HashContextMD5  oCurrentHashContextMD5;
};

// ------------------------------------------------------------------------------------------------
//                                           EWF FORMAT
// ------------------------------------------------------------------------------------------------

#if (ENABLE_LIBEWF)

#if (LIBEWF_VERSION >= 20130416)
   #define CHK_LIBEWF(Fn)                                                 \
   {                                                                      \
      int rclibewf = (Fn);                                                \
      if (rclibewf != 1)                                                  \
      {                                                                   \
         const int BuffLen = 1024;                                        \
         char    *pBuff    = (char*) malloc(BuffLen);                     \
         libewf_error_sprint (poLibewfErr, pBuff, BuffLen);                \
         LOG_ERROR ("[%s] Error in libewf function: %s, rc=%d [%s]", QSTR_TO_PSZ (poDevice->LinuxDevice), #Fn, rclibewf, pBuff) \
         free(pBuff);                                                     \
         return ERROR_THREADWRITE_LIBEWF_FAILED;                          \
      }                                                                   \
   }
#else
   #define CHK_LIBEWF(Fn)                                                 \
   {                                                                      \
      int rclibewf = (Fn);                                                \
      if (rclibewf != 1)                                                  \
      {                                                                   \
         LOG_ERROR ("[%s] Error in libewf function: %s, rc=%d", QSTR_TO_PSZ (poDevice->LinuxDevice), #Fn, rclibewf) \
         return ERROR_THREADWRITE_LIBEWF_FAILED;                          \
      }                                                                   \
   }
#endif

class t_OutputFileEWF: public t_OutputFile
{
   public:
      t_OutputFileEWF (t_ThreadWrite *pThreadWrite):
          t_OutputFile (pThreadWrite)
      {
         poFile                  = nullptr;
         #if (LIBEWF_VERSION >= 20130416)
            poLibewfErr          = nullptr;
         #endif
         poAcquisition           = nullptr;
         poDevice                = nullptr;   //lint -esym(613,t_OutputFileEWF::poDevice)  Prevent lint from telling us about possible null pointers in the following code
         poVerifyBuff            = nullptr;
         poImageFilenameArr      = nullptr;
          oImageFileCount        = 0;
          oHasCompressionThreads = false;
          oVerification          = false;
      }

      ~t_OutputFileEWF (void)
      {
         if (t_OutputFileEWF::Opened())
            (void) t_OutputFileEWF::Close();
      } //lint !e1740

      APIRET Open (t_pAcquisition pAcquisition, bool Verify)
      {
         QString          Uname;
         QString          GuymagerVersion;
         LIBEWF_HANDLE  *pFile=nullptr;
         char           *pAsciiFileName;
         QByteArray       AsciiFileName = (pAcquisition->ImagePath + pAcquisition->ImageFilename).toLatin1();

         poAcquisition          = pAcquisition;
         poDevice               = pAcquisition->pDevice;
         oHasCompressionThreads = poDevice->HasCompressionThreads();
         oVerification          = Verify;

         #if (LIBEWF_VERSION >= 20130416)
            if (libewf_handle_initialize(&pFile, &poLibewfErr) != 1)
            {
               LOG_INFO ("[%s] Error while initialising libewf handle", QSTR_TO_PSZ (poDevice->LinuxDevice))
               return ERROR_THREADWRITE_OPEN_FAILED;
            }
         #endif

         if (Verify)
         {
            QFileInfoList FileInfoList;
            QString       ExtensionImage;
            QFileInfo     FileInfo;
            QDir          Dir(poAcquisition->ImagePath);
            int           i;

            CHK (t_File::GetFormatExtension (poDevice, &ExtensionImage))
            FileInfoList = Dir.entryInfoList (QStringList(pAcquisition->ImageFilename + ExtensionImage), QDir::Files, QDir::Name);
            oImageFileCount = FileInfoList.count();
            poImageFilenameArr = (char **) malloc (oImageFileCount * sizeof (char*));
            for (i=0; i<oImageFileCount; i++)
            {
               FileInfo       = FileInfoList.takeFirst();
               AsciiFileName  = FileInfo.absoluteFilePath().toLatin1();
               pAsciiFileName = AsciiFileName.data();
               poImageFilenameArr[i] = (char *) malloc (strlen(pAsciiFileName)+1);
               strcpy (poImageFilenameArr[i], pAsciiFileName);
            }

            CHK_EXIT (poThreadWrite->SetDebugMessage ("Verification: Calling libewf_open"))
            #if (LIBEWF_VERSION >= 20130416)
               if (libewf_handle_open(pFile, poImageFilenameArr, oImageFileCount, LIBEWF_ACCESS_FLAG_READ, &poLibewfErr) != 1)
                  pFile = nullptr;
            # else
               pFile = libewf_open (poImageFilenameArr, oImageFileCount, libewf_get_flags_read());
            #endif
            CHK_EXIT (poThreadWrite->SetDebugMessage ("Verification: Returning from libewf_open"))
            if (pFile == nullptr)
            {
               LOG_INFO ("[%s] Error while reopening EWF for verification. The files are:", QSTR_TO_PSZ (poDevice->LinuxDevice))
               for (i=0; i<oImageFileCount; i++)
                  LOG_INFO ("%s", poImageFilenameArr[i])
               return ERROR_THREADWRITE_OPEN_FAILED;
            }
         }
         else
         {
            char *pAsciiFileName = AsciiFileName.data();

            #if (LIBEWF_VERSION >= 20130416)
               if (libewf_handle_open(pFile, &pAsciiFileName, 1, LIBEWF_OPEN_WRITE, &poLibewfErr) != 1)
                  pFile = nullptr;
            #else
               pFile = libewf_open (&pAsciiFileName, 1, LIBEWF_OPEN_WRITE);
            #endif

            if (pFile == nullptr)
               return ERROR_THREADWRITE_OPEN_FAILED;

            CHK (ToolSysInfoUname (Uname))
            GuymagerVersion = QString("guymager ") + QString(pCompileInfoVersion);
            if (CONFIG (AvoidEncaseProblems))
            {
               GuymagerVersion = GuymagerVersion.left (THREADWRITE_ENCASE_MAXLEN_IMAGERVERSION);
               Uname           = Uname          .left (THREADWRITE_ENCASE_MAXLEN_OSVERSION    );
            }


            #if (LIBEWF_VERSION >= 20130416)
               #define MOD_QSTR(QStr) (const uint8_t *)QStr.toUtf8().data(), strlen(QStr.toUtf8().data())
               #define MOD_STR(pStr)  (const uint8_t *)pStr, strlen((const char *)pStr)
               #define SET_UTF8_STRING(Field,Value)                                                                     \
                  if (Value.length())        /* libewf fails when setting an empty string, so let's check this here */  \
                     CHK_LIBEWF (libewf_handle_set_utf8_header_value (pFile, MOD_STR(Field), MOD_QSTR(Value), &poLibewfErr))

               uint8_t media_flags = 0;

               CHK_LIBEWF (libewf_handle_set_format              (pFile, (uint8_t) CONFIG (EwfFormat)                  , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_media_size          (pFile, poDevice->Size                                , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_bytes_per_sector    (pFile, (unsigned int) poDevice->SectorSize           , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_sectors_per_chunk   (pFile, poDevice->FifoBlockSize / poDevice->SectorSize, &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_maximum_segment_size(pFile, poAcquisition->SplitFileSize                  , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_compression_values  (pFile, CONFIG (EwfCompression), 0                    , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_media_type          (pFile, poDevice->Removable ? LIBEWF_MEDIA_TYPE_REMOVABLE : LIBEWF_MEDIA_TYPE_FIXED, &poLibewfErr))
               CHK_LIBEWF (libewf_handle_get_media_flags         (pFile, &media_flags                                  , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_media_flags         (pFile, media_flags | LIBEWF_MEDIA_FLAG_PHYSICAL      , &poLibewfErr))
               CHK_LIBEWF (libewf_handle_set_error_granularity   (pFile, 1                                             , &poLibewfErr))

               SET_UTF8_STRING("case_number"             , poAcquisition->CaseNumber    )
               SET_UTF8_STRING("description"             , poAcquisition->Description   )
               SET_UTF8_STRING("examiner_name"           , poAcquisition->Examiner      )
               SET_UTF8_STRING("evidence_number"         , poAcquisition->EvidenceNumber)
               SET_UTF8_STRING("notes"                   , poAcquisition->Notes         )
               SET_UTF8_STRING("acquiry_operating_system", Uname                        )
               SET_UTF8_STRING("acquiry_software_version", GuymagerVersion              )
            #else
               #define MOD_QSTR(QStr) QStr.toLatin1().data(), strlen(QStr.toLatin1().data())
               CHK_LIBEWF (libewf_set_format             (pFile, (uint8_t) CONFIG (EwfFormat)))
               CHK_LIBEWF (libewf_set_media_size         (pFile, poDevice->Size))
               CHK_LIBEWF (libewf_set_bytes_per_sector   (pFile, (unsigned int) poDevice->SectorSize))
               CHK_LIBEWF (libewf_set_sectors_per_chunk  (pFile, poDevice->FifoBlockSize / poDevice->SectorSize))
               CHK_LIBEWF (libewf_set_segment_file_size  (pFile, poAcquisition->SplitFileSize))
               CHK_LIBEWF (libewf_set_compression_values (pFile, CONFIG (EwfCompression), 0)) // last parameter must be set to 0, else, only empty-block compression is done
               CHK_LIBEWF (libewf_set_media_type         (pFile, poDevice->Removable ? LIBEWF_MEDIA_TYPE_REMOVABLE : LIBEWF_MEDIA_TYPE_FIXED))
               CHK_LIBEWF (libewf_set_volume_type        (pFile, LIBEWF_VOLUME_TYPE_PHYSICAL))
               CHK_LIBEWF (libewf_set_error_granularity  (pFile, 1))

               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"case_number"             , MOD_QSTR(poAcquisition->CaseNumber    )))
               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"description"             , MOD_QSTR(poAcquisition->Description   )))
               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"examiner_name"           , MOD_QSTR(poAcquisition->Examiner      )))
               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"evidence_number"         , MOD_QSTR(poAcquisition->EvidenceNumber)))
               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"notes"                   , MOD_QSTR(poAcquisition->Notes         )))
               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"acquiry_operating_system", MOD_QSTR(Uname                        )))
               CHK_LIBEWF (libewf_set_header_value (pFile, (char *)"acquiry_software_version", MOD_QSTR(GuymagerVersion              )))
            #endif
            #undef MOD_STR
            #undef MOD_QSTR

         }
         poFile = pFile; // Only set poFile at the very end, so the CompressionThreads won't use it until everything is initialised
         return NO_ERROR;
      }

      APIRET Write (t_pFifoBlock pFifoBlock)
      {
         int Written;
         int Size;

         if (oHasCompressionThreads != pFifoBlock->EwfPreprocessed)
            CHK (ERROR_THREADWRITE_WRONG_BLOCK)

         if (oHasCompressionThreads)
         {
            Size = pFifoBlock->EwfDataSize;
            #if (LIBEWF_VERSION >= 20130416)
               uint8_t checksum_buffer[ 4 ];
               Written = libewf_handle_write_chunk(poFile, pFifoBlock->Buffer, Size, pFifoBlock->DataSize,
                                                                                     pFifoBlock->EwfCompressionUsed,
                                                                                     checksum_buffer,
                                                                                     pFifoBlock->EwfChunkCRC,
                                                                                     pFifoBlock->EwfWriteCRC,
                                                                                     nullptr);
            #else
               Written = libewf_raw_write_buffer (poFile, pFifoBlock->Buffer, Size, pFifoBlock->DataSize,
                                                                                    pFifoBlock->EwfCompressionUsed,
                                                                                    pFifoBlock->EwfChunkCRC,
                                                                                    pFifoBlock->EwfWriteCRC);
            #endif
         }
         else
         {
            Size = pFifoBlock->DataSize;
            #if (LIBEWF_VERSION >= 20130416)
               Written = libewf_handle_write_buffer (poFile, pFifoBlock->Buffer, Size, nullptr);
            #else
               Written = libewf_write_buffer (poFile, pFifoBlock->Buffer, Size);
            #endif
         }

         if (Written != Size)
         {
            LOG_ERROR ("[%s] Written %d/%d bytes", QSTR_TO_PSZ (poDevice->LinuxDevice), Written, Size)
            return ERROR_THREADWRITE_WRITE_FAILED;
         }

         return NO_ERROR;
      }

      APIRET Verify (t_pHashContextMD5 pHashContextMD5, t_pHashContextSHA1 pHashContextSHA1, t_pHashContextSHA256 pHashContextSHA256, t_ImageFileHashList &ImageFileHashList, quint64 *pPos)
      {
         quint64      Remaining;
         unsigned int ToRead;
         ssize_t      Read;


         if (*pPos == 0)
         {
            poVerifyBuff = UTIL_MEM_ALLOC (poDevice->FifoBlockSize);
            if (poVerifyBuff == nullptr)
               CHK (ERROR_THREADWRITE_MALLOC_FAILED);

            for (int i=0; i<oImageFileCount; i++)
               ImageFileHashList.append (new t_ImageFileHash (poImageFilenameArr[i], nullptr));  // No MD5 files hashes for EWF files generated by libewf
         }
         Remaining = poDevice->Size - *pPos;
         ToRead = GETMIN (Remaining, poDevice->FifoBlockSize);

         CHK_EXIT (poThreadWrite->SetDebugMessage ("Verification: Calling libewf_read_buffer"))
         #if (LIBEWF_VERSION >= 20130416)
            Read = libewf_handle_read_buffer (poFile, poVerifyBuff, ToRead, nullptr);
         #else
            Read = libewf_read_buffer (poFile, poVerifyBuff, ToRead);
         #endif
         CHK_EXIT (poThreadWrite->SetDebugMessage ("Verification: Returning from libewf_read_buffer"))

         if (Read != (ssize_t)ToRead)
         {
            LOG_INFO ("[%s] Reading from EWF file failed (%zu)", QSTR_TO_PSZ (poDevice->LinuxDevice), Read);
            return ERROR_THREADWRITE_VERIFY_FAILED;
         }
         if (pHashContextMD5)    CHK_EXIT (HashMD5Append    (pHashContextMD5   , poVerifyBuff, ToRead))
         if (pHashContextSHA1)   CHK_EXIT (HashSHA1Append   (pHashContextSHA1  , poVerifyBuff, ToRead))
         if (pHashContextSHA256) CHK_EXIT (HashSHA256Append (pHashContextSHA256, poVerifyBuff, ToRead))

         *pPos += ToRead;

         return NO_ERROR;
      }

      APIRET Close (void)
      {
         QList<quint64>   BadSectors;
         quint64          i, Count;
         quint64          From, To, Next;
         int              rc;
         int              j;

         if (poFile == nullptr)
            CHK (ERROR_THREADWRITE_NOT_OPENED)

         // Write bad sector list
         // ---------------------
         if (!oVerification)
         {
            CHK (poDevice->GetBadSectors (BadSectors, false))
            Count = BadSectors.count();
            if (Count)
            {
               i = 0;
               while (i<Count)
               {
                  From = BadSectors.at(i++);
                  To   = From;
                  while (i < Count)
                  {
                     Next = BadSectors.at(i);
                     if (Next != To+1)
                         break;
                     To = Next;
                     i++;
                  }
                  #if (LIBEWF_VERSION >= 20130416)
                     CHK_LIBEWF (libewf_handle_append_acquiry_error (poFile, From, To - From + 1, nullptr))
                  #else
                     CHK_LIBEWF (libewf_add_acquiry_error(poFile, From, To - From + 1))
                  #endif
               }
            }
         }

         // Write hash
         // ----------
         if (!oVerification && poDevice->HasCompressionThreads())
         {
            if (poAcquisition->CalcMD5)
            {
               #if (LIBEWF_VERSION >= 20130416)
                  CHK_LIBEWF (libewf_handle_set_md5_hash (poFile, (uint8_t*)&poDevice->MD5Digest, HASH_MD5_DIGEST_LENGTH, nullptr))
               #else
                  CHK_LIBEWF (libewf_set_md5_hash (poFile, (uint8_t*)&poDevice->MD5Digest, HASH_MD5_DIGEST_LENGTH))
               #endif
            }
            if (poAcquisition->CalcSHA1)
            {
               #if (LIBEWF_VERSION >= 20130416)
                  CHK_LIBEWF (libewf_handle_set_sha1_hash (poFile, (uint8_t*)&poDevice->SHA1Digest, HASH_SHA1_DIGEST_LENGTH, nullptr))
               #else
                  CHK_LIBEWF (libewf_set_sha1_hash (poFile, (uint8_t*)&poDevice->SHA1Digest, HASH_SHA1_DIGEST_LENGTH))
               #endif
            }
         }

         // Close and cleanup
         // -----------------
         #if (LIBEWF_VERSION >= 20130416)
            rc = libewf_handle_close(poFile, nullptr);
            if (rc != 0)
            {
               LOG_ERROR ("[%s] Error in libewf function: libewf_handle_close, rc=%d", QSTR_TO_PSZ (poDevice->LinuxDevice), rc)
               return ERROR_THREADWRITE_LIBEWF_FAILED;
            }
            if (libewf_handle_free(&poFile, nullptr) != 1)
            {
               LOG_ERROR ("[%s] Error in libewf function: libewf_handle_free, rc=%d", QSTR_TO_PSZ (poDevice->LinuxDevice), rc)
               return ERROR_THREADWRITE_LIBEWF_FAILED;
            }
         #else
            rc = libewf_close (poFile);
            if (rc != 0)
            {
               LOG_ERROR ("[%s] Error in libewf function: libewf_close, rc=%d", QSTR_TO_PSZ (poDevice->LinuxDevice), rc)
               return ERROR_THREADWRITE_LIBEWF_FAILED;
            }
         #endif

         if (poVerifyBuff)
         {
            UTIL_MEM_FREE (poVerifyBuff);
            poVerifyBuff = nullptr;
         }

         for (j=0; j<oImageFileCount; j++)
            free (poImageFilenameArr[j]);
         free (poImageFilenameArr);

         poFile = nullptr;
         return NO_ERROR;
      }

      void * GetFileHandle (void)
      {
         return poFile;
      }

      bool Opened (void)
      {
         return (poFile != nullptr);
      }

   private:
      LIBEWF_HANDLE *poFile;
      #if (LIBEWF_VERSION >= 20130416)
         libewf_error_t *poLibewfErr;
      #endif
      t_pAcquisition poAcquisition;
      t_pDevice      poDevice;
      bool            oVerification;
      void          *poVerifyBuff;
      int             oImageFileCount;
      char         **poImageFilenameArr;
      bool            oHasCompressionThreads;
};

#endif // ENABLE_LIBEWF

// ------------------------------------------------------------------------------------------------
//                                           AAFF FORMAT
// ------------------------------------------------------------------------------------------------

class t_OutputFileAAFF: public t_OutputFile
{
   public:
      t_OutputFileAAFF (t_ThreadWrite *pThreadWrite) :
         t_OutputFile           (pThreadWrite),
         poFile                 (nullptr),
         poAcquisition          (nullptr),
         poDevice               (nullptr),
         poVerifyBuff           (nullptr),
         oHasCompressionThreads (false),
         oVerification          (false)
      {
      }

      ~t_OutputFileAAFF (void)
      {
         if (t_OutputFileAAFF::Opened())
            (void) t_OutputFileAAFF::Close();
      } //lint !e1740

      APIRET Open (t_pAcquisition pAcquisition, bool Verify)
      {
         QString               Extension;
         QString               FileName;
         QString               Mac;
         QString               DateTimeStr;
         t_ToolSysInfoMacAddr  MacAddr;
         char                *pCommandLine;
         t_pAaff              pFile;
         APIRET                rc;

         poAcquisition = pAcquisition;
         poDevice      = pAcquisition->pDevice;
         oVerification =  Verify;
         oHasCompressionThreads = poDevice->HasCompressionThreads();

         CHK (t_File::GetFormatExtension (poDevice, &Extension))
         FileName = pAcquisition->ImagePath + pAcquisition->ImageFilename + Extension;

         if (oVerification)
         {
            rc = AaffOpen (&pFile, QSTR_TO_PSZ(FileName), poDevice->Size);
            if (rc)
            {
               LOG_INFO ("[%s] AFF image verification failed: %s", QSTR_TO_PSZ (poDevice->LinuxDevice), ToolErrorMessage (rc))
               return ERROR_THREADWRITE_VERIFY_FAILED;
            }
         }
         else
         {
            rc = AaffOpen (&pFile, QSTR_TO_PSZ(FileName), poDevice->Size, poDevice->SectorSize, poDevice->FifoBlockSize);
            if (rc)
            {
               LOG_INFO ("[%s] Opening AFF image for writing failed: %s", QSTR_TO_PSZ (poDevice->LinuxDevice), ToolErrorMessage (rc))
               return ERROR_THREADWRITE_OPEN_FAILED;
            }

            rc = ToolSysInfoGetMacAddr (&MacAddr);
            if (rc == TOOLSYSINFO_ERROR_NO_ADDR)
            {
               Mac = "none";
            }
            else
            {
               CHK (rc)
               Mac = &MacAddr.AddrStr[0];
            }

            DateTimeStr  = poDevice->StartTimestamp.toString ("yyyy-MM-dd hh:mm:ss");
            DateTimeStr += " localtime";
            CHK (MainGetCommandLine (&pCommandLine))

            CHK (AaffWriteSegmentStr (pFile, AAFF_SEGNAME_COMMAND_LINE, 0, pCommandLine))
            CHK (AaffWriteSegmentStr (pFile, AAFF_SEGNAME_MACADDR     , 0, QSTR_TO_PSZ(Mac                          )))
            CHK (AaffWriteSegmentStr (pFile, AAFF_SEGNAME_DATE        , 0, QSTR_TO_PSZ(DateTimeStr                  )))
            CHK (AaffWriteSegmentStr (pFile, AAFF_SEGNAME_DEVICE      , 0, QSTR_TO_PSZ(poDevice->LinuxDevice        )))
            CHK (AaffWriteSegmentStr (pFile, AAFF_SEGNAME_MODEL       , 0, QSTR_TO_PSZ(poDevice->Model              )))
            CHK (AaffWriteSegmentStr (pFile, AAFF_SEGNAME_SN          , 0, QSTR_TO_PSZ(poDevice->SerialNumber       )))
            CHK (AaffWriteSegmentStr (pFile, "CaseNumber"             , 0, QSTR_TO_PSZ(poAcquisition->CaseNumber    )))
            CHK (AaffWriteSegmentStr (pFile, "EvidenceNumber"         , 0, QSTR_TO_PSZ(poAcquisition->EvidenceNumber)))
            CHK (AaffWriteSegmentStr (pFile, "Examiner"               , 0, QSTR_TO_PSZ(poAcquisition->Examiner      )))
            CHK (AaffWriteSegmentStr (pFile, "Description"            , 0, QSTR_TO_PSZ(poAcquisition->Description   )))
            CHK (AaffWriteSegmentStr (pFile, "Notes"                  , 0, QSTR_TO_PSZ(poAcquisition->Notes         )))
         }

         poFile = pFile; // Only set poFile at the very end, so the CompressionThreads won't use it until everything is initialised
         return NO_ERROR;
      }

      //lint -save -esym(613,t_OutputFileAFF::poDevice)   Possible use of null pointer
      APIRET Write (t_pFifoBlock pFifoBlock)
      {
         APIRET rc;
         if (!oHasCompressionThreads) // Do preprocessing if not already done in compression threads
         {
            t_pFifoBlock pPreprocessBlock;

            CHK_EXIT (t_Fifo::Create (poDevice->pFifoMemory, pPreprocessBlock, poDevice->FifoAllocBlockSize))
            pPreprocessBlock->DataSize = pFifoBlock->DataSize;
            CHK_EXIT (AaffPreprocess (&pPreprocessBlock->AaffPreprocess, pFifoBlock->Buffer, pFifoBlock->DataSize, pPreprocessBlock->Buffer, pPreprocessBlock->BufferSize))
            if (pPreprocessBlock->AaffPreprocess.Compressed)
            {
               CHK_EXIT (t_Fifo::Destroy (poDevice->pFifoMemory, pFifoBlock))
               pFifoBlock = pPreprocessBlock;
            }
            else
            {
               pFifoBlock->AaffPreprocess = pPreprocessBlock->AaffPreprocess;
               CHK_EXIT (t_Fifo::Destroy (poDevice->pFifoMemory, pPreprocessBlock))
            }
         }

         rc = AaffWrite (poFile, &pFifoBlock->AaffPreprocess, pFifoBlock->Buffer, pFifoBlock->DataSize);
         if (rc != NO_ERROR)
            CHK (ERROR_THREADWRITE_WRITE_FAILED)

         return NO_ERROR;
      }
      //lint -restore

      APIRET Verify (t_pHashContextMD5 pHashContextMD5, t_pHashContextSHA1 pHashContextSHA1, t_pHashContextSHA256 pHashContextSHA256, t_ImageFileHashList &ImageFileHashList, quint64 *pPos)
      {
         unsigned int    DataLen = poDevice->FifoBlockSize;
         APIRET          rc;
         QString         ImageFilename;
         t_HashMD5Digest MD5Digest;
         bool            MD5Valid;

         if (*pPos == 0)
         {
            poVerifyBuff = UTIL_MEM_ALLOC (poDevice->FifoBlockSize);
            if (poVerifyBuff == nullptr)
               CHK (ERROR_THREADWRITE_MALLOC_FAILED);
         }
         rc = AaffReadNextPage (poFile, (unsigned char *) poVerifyBuff, &DataLen, &ImageFilename, &MD5Digest, &MD5Valid);
         if (rc)
         {
            LOG_INFO ("[%s] AFF image verification failed: %s", QSTR_TO_PSZ (poDevice->LinuxDevice), ToolErrorMessage (rc))
            return ERROR_THREADWRITE_VERIFY_FAILED;
         }

         if (pHashContextMD5)    CHK_EXIT (HashMD5Append    (pHashContextMD5   , poVerifyBuff, DataLen))
         if (pHashContextSHA1)   CHK_EXIT (HashSHA1Append   (pHashContextSHA1  , poVerifyBuff, DataLen))
         if (pHashContextSHA256) CHK_EXIT (HashSHA256Append (pHashContextSHA256, poVerifyBuff, DataLen))

         *pPos += DataLen;

         if (!ImageFilename.isEmpty())
         {
            QString MD5Str;

            CHK (HashMD5DigestStr (&MD5Digest, MD5Str))
            ImageFileHashList.append (new t_ImageFileHash (ImageFilename, &MD5Digest, MD5Valid));
         }

         return NO_ERROR;
      }

      //lint -save -esym(613,t_OutputFileAFF::poDevice)   Possible use of null pointer
      APIRET Close (void)
      {
         QList<quint64> BadSectors;
         int            Seconds;

         if (poFile == nullptr)
            CHK (ERROR_THREADWRITE_NOT_OPENED)

         if (oVerification)
         {
            CHK (AaffClose (&poFile))
            if (poVerifyBuff)
            {
               UTIL_MEM_FREE (poVerifyBuff);
               poVerifyBuff = nullptr;
            }
         }
         else
         {
            CHK (poDevice->GetBadSectors (BadSectors, false))
            Seconds  = poDevice->StartTimestamp.secsTo (QDateTime::currentDateTime());

            CHK (AaffClose (&poFile, BadSectors.count(), (unsigned char*)&poDevice->MD5Digest, (unsigned char*)&poDevice->SHA1Digest, (unsigned char*)&poDevice->SHA256Digest, Seconds))
         }

         poFile = nullptr;

         return NO_ERROR;
      }
      //lint -restore

      void *GetFileHandle (void)
      {
         return poFile;
      }

      bool Opened (void)
      {
         return (poFile != nullptr);
      }

   private:
      t_pAaff        poFile;
      t_pAcquisition poAcquisition;
      t_pDevice      poDevice;
      void          *poVerifyBuff;
      bool            oHasCompressionThreads;
      bool            oVerification;
};

// ------------------------------------------------------------------------------------------------
//                                           AEWF FORMAT
// ------------------------------------------------------------------------------------------------

class t_OutputFileAEWF: public t_OutputFile
{
   public:
      t_OutputFileAEWF (t_ThreadWrite *pThreadWrite) :
         t_OutputFile           (pThreadWrite),
         poFile                 (nullptr),
         poAcquisition          (nullptr),
         poDevice               (nullptr),
         poVerifyBuff           (nullptr),
          oHasCompressionThreads(false),
          oVerification         (false)
      {
      }

      ~t_OutputFileAEWF (void)
      {
         if (t_OutputFileAEWF::Opened())
            (void) t_OutputFileAEWF::Close();
      } //lint !e1740

      APIRET Open (t_pAcquisition pAcquisition, bool Verify)
      {
         t_pAewf pFile = nullptr;
         QString  FileName;
         QString  GuymagerVersion;
         QString  Uname;
         struct   utsname SystemVersion;
         APIRET   rc;

         poAcquisition = pAcquisition;
         poDevice      = pAcquisition->pDevice;
         oVerification =  Verify;
         oHasCompressionThreads = poDevice->HasCompressionThreads();

         FileName = poAcquisition->ImagePath + pAcquisition->ImageFilename;
         if (oVerification)
         {
            rc = AewfOpen (&pFile, QSTR_TO_PSZ(FileName));
            if (rc)
            {
               LOG_INFO ("[%s] Opening AEWF image for verification failed: %s", QSTR_TO_PSZ (poDevice->LinuxDevice), ToolErrorMessage (rc))
               return ERROR_THREADWRITE_OPEN_FAILED;
            }
         }
         else
         {
            if (uname (&SystemVersion) == -1)
               return ERROR_THREADWRITE_OSVERSION_QUERY_FAILED;

            Uname = QString(SystemVersion.release);
            GuymagerVersion = QString("guymager ") + QString(pCompileInfoVersion);
            if (CONFIG (AvoidEncaseProblems))
            {
               GuymagerVersion = GuymagerVersion.left (THREADWRITE_ENCASE_MAXLEN_IMAGERVERSION);
               Uname           = Uname          .left (THREADWRITE_ENCASE_MAXLEN_OSVERSION    );
            }
            rc = AewfOpen (&pFile, QSTR_TO_PSZ(FileName), poDevice->Size, pAcquisition->SplitFileSize, poDevice->FifoBlockSize, poDevice->SectorSize,
                                   poDevice->Removable ? AEWF_MEDIATYPE_REMOVABLE : AEWF_MEDIATYPE_FIXED,
                                   AEWF_MEDIAFLAGS_PHYSICAL,
                                   pAcquisition->Description, pAcquisition->CaseNumber, pAcquisition->EvidenceNumber,
                                   pAcquisition->Examiner   , pAcquisition->Notes     , poDevice->Model             ,
                                   poDevice->SerialNumber   , GuymagerVersion         , Uname);
            if (rc)
            {
               LOG_INFO ("[%s] Opening AEWF image for writing failed: %s", QSTR_TO_PSZ (poDevice->LinuxDevice), ToolErrorMessage (rc))
               return ERROR_THREADWRITE_OPEN_FAILED;
            }
         }

         poFile = pFile; // Only set poFile at the very end, so the CompressionThreads won't use it until everything is initialised
         return NO_ERROR;
      }

      APIRET Write (t_pFifoBlock pFifoBlock)
      {
         APIRET rc;

         if (!oHasCompressionThreads) // Do preprocessing if not already done in compression threads
         {
            t_pFifoBlock pPreprocessBlock;

            CHK_EXIT (t_Fifo::Create (poDevice->pFifoMemory, pPreprocessBlock, poDevice->FifoAllocBlockSize))
            pPreprocessBlock->DataSize = pFifoBlock->DataSize;
            CHK_EXIT (AewfPreprocess (&pPreprocessBlock->AewfPreprocess, pFifoBlock      ->Buffer, pFifoBlock      ->DataSize,
                                                                         pPreprocessBlock->Buffer, pPreprocessBlock->BufferSize))
            if (pPreprocessBlock->AewfPreprocess.Compressed)
            {
               CHK_EXIT (t_Fifo::Destroy (poDevice->pFifoMemory, pFifoBlock))
               pFifoBlock = pPreprocessBlock;
            }
            else
            {
               pFifoBlock->AewfPreprocess = pPreprocessBlock->AewfPreprocess;
               CHK_EXIT (t_Fifo::Destroy (poDevice->pFifoMemory, pPreprocessBlock))
            }
         }
         rc = AewfWrite (poFile, &pFifoBlock->AewfPreprocess, pFifoBlock->Buffer, pFifoBlock->AewfPreprocess.DataLenOut);
         if (rc != NO_ERROR)
            CHK (ERROR_THREADWRITE_WRITE_FAILED)

         return NO_ERROR;
      }
      //lint -restore

   private:
      APIRET ProcessUnit (t_pHashContextMD5 pHashContextMD5, t_pHashContextSHA1 pHashContextSHA1, t_pHashContextSHA256 pHashContextSHA256, t_ImageFileHashList &ImageFileHashList, quint64 *pPos, bool *pFinished)
      {
         QString         SegmentFilename;
         QString         MD5Str;
         t_HashMD5Digest MD5Digest;
         bool            MD5Valid;
         unsigned int    DataLen;
         APIRET          rc;

         DataLen = poDevice->FifoAllocBlockSize;
         rc = AewfReadNextChunk (poFile, (unsigned char *) poVerifyBuff, &DataLen, &SegmentFilename, &MD5Digest, &MD5Valid, pFinished);
         if (rc)
         {
            LOG_INFO ("[%s] AEWF image verification failed: %s", QSTR_TO_PSZ (poDevice->LinuxDevice), ToolErrorMessage (rc))
            return ERROR_THREADWRITE_VERIFY_FAILED;
         }

         if (DataLen)
         {
            if (pHashContextMD5)    CHK_EXIT (HashMD5Append    (pHashContextMD5   , poVerifyBuff, DataLen))
            if (pHashContextSHA1)   CHK_EXIT (HashSHA1Append   (pHashContextSHA1  , poVerifyBuff, DataLen))
            if (pHashContextSHA256) CHK_EXIT (HashSHA256Append (pHashContextSHA256, poVerifyBuff, DataLen))
            *pPos += DataLen;
         }

         if (!SegmentFilename.isEmpty())
         {
            CHK (HashMD5DigestStr (&MD5Digest, MD5Str))
            ImageFileHashList.append (new t_ImageFileHash (SegmentFilename, &MD5Digest, MD5Valid));
         }
         return NO_ERROR;
      }

   public:
      APIRET Verify (t_pHashContextMD5 pHashContextMD5, t_pHashContextSHA1 pHashContextSHA1, t_pHashContextSHA256 pHashContextSHA256, t_ImageFileHashList &ImageFileHashList, quint64 *pPos)
      {
         bool Finished;

         if (*pPos == 0)
         {
            poVerifyBuff = UTIL_MEM_ALLOC (poDevice->FifoAllocBlockSize);
            if (poVerifyBuff == nullptr)
               CHK (ERROR_THREADWRITE_MALLOC_FAILED);
         }
         CHK (ProcessUnit (pHashContextMD5, pHashContextSHA1, pHashContextSHA256, ImageFileHashList, pPos, &Finished))

         if (*pPos >= poDevice->Size)  // If all data has been read keep calling
         {                             // the processing function until it says it
            while (!Finished)          // finished (see AewfReadNextChunk)
               CHK (ProcessUnit (pHashContextMD5, pHashContextSHA1, pHashContextSHA256, ImageFileHashList, pPos, &Finished))
         }
         return NO_ERROR;
      }

      APIRET Close (void)
      {
         QList<quint64> BadSectors;

         if (poFile == nullptr)
            CHK (ERROR_THREADWRITE_NOT_OPENED)

         if (oVerification)
         {
            CHK (AewfClose (&poFile))
         }
         else
         {
            CHK (poDevice->GetBadSectors (BadSectors, false))
            CHK (AewfClose (&poFile, BadSectors, (unsigned char*)&poDevice->MD5Digest, (unsigned char*)&poDevice->SHA1Digest, (unsigned char*)&poDevice->SHA256Digest))
         }
         poFile = nullptr;

         return NO_ERROR;
      }
      //lint -restore

      void *GetFileHandle (void)
      {
         return poFile;
      }

      bool Opened (void)
      {
         return (poFile != nullptr);
      }

   private:
      t_pAewf        poFile;
      t_pAcquisition poAcquisition;
      t_pDevice      poDevice;
      void          *poVerifyBuff;
      bool            oHasCompressionThreads;
      bool            oVerification;
};


// ------------------------------------------------------------------------------------------------
//                                           WRITE THREAD
// ------------------------------------------------------------------------------------------------

class t_ThreadWriteLocal
{
   public:
      t_ThreadWriteLocal (void) :
         pOutputFile        (nullptr),
         pSlowDownRequest   (nullptr),
         pAcquisition       (nullptr),
         FileHandleRequests (0)
      {}

   public:
      t_OutputFile  *pOutputFile;
      bool          *pSlowDownRequest;
      t_pAcquisition pAcquisition;
      t_pFifo      *ppFifo;
      int             FileHandleRequests;
      QMutex          SemHandle;
};


t_ThreadWrite::t_ThreadWrite(void)
{
   CHK_EXIT (ERROR_THREADWRITE_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 not initialised

t_ThreadWrite::t_ThreadWrite (t_pAcquisition pAcquisition, t_pFifo *ppFifo, bool *pSlowDownRequest)
   :pOwn(nullptr)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_OPEN_FAILED              ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_WRITE_FAILED             ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_VERIFY_FAILED            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_CLOSE_FAILED             ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_NOT_OPENED               ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_INVALID_FORMAT           ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_CONSTRUCTOR_NOT_SUPPORTED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_WRONG_BLOCK              ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_OUT_OF_SEQUENCE_BLOCK    ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_HANDLE_NOT_YET_AVAILABLE ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_HANDLE_TIMEOUT           ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_LIBEWF_FAILED            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_MALLOC_FAILED            ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_THREADWRITE_OSVERSION_QUERY_FAILED   ))
   }

   pOwn = new t_ThreadWriteLocal;
   pOwn->pAcquisition       = pAcquisition;
   pOwn->ppFifo             = ppFifo;
   pOwn->pOutputFile        = nullptr;
   pOwn->pSlowDownRequest   = pSlowDownRequest;
   pOwn->FileHandleRequests = 0;

   CHK_QT_EXIT (connect (this, SIGNAL(finished()), this, SLOT(SlotFinished())))
}

t_ThreadWrite::~t_ThreadWrite (void)
{
   delete pOwn;
}


static APIRET ThreadWriteDebugCheckLibewfData (t_pFifoBlock pFifoBlock)
{
   unsigned char *pBufferUncompressed;
   uLongf          UncompressedSize;
   int             zrc=99;
   uint32_t        CRC1;
   uint32_t        CRC2;
   bool            Error;

   Error = !pFifoBlock->EwfPreprocessed;

   // Get the original data
   // ---------------------
   if (pFifoBlock->EwfCompressionUsed)
   {
      UncompressedSize = pFifoBlock->DataSize + 4096; // DataSize should be enough for the uncompression buffer, as the uncompressed data originally had that
                                                      // size, but as this fn is used for debugging and we do not know what the bug might be, we add 4K for safety
      pBufferUncompressed = (unsigned char *) UTIL_MEM_ALLOC (UncompressedSize);
      if (pBufferUncompressed == nullptr)
      {
         LOG_ERROR ("malloc returned nullptr")
         CHK_EXIT (1967)
      }
      zrc = uncompress ((Bytef*)pBufferUncompressed, &UncompressedSize, pFifoBlock->Buffer, pFifoBlock->EwfDataSize);
      Error = Error || (zrc != Z_OK);
      Error = Error || (UncompressedSize != (unsigned) pFifoBlock->DataSize);

      ((char *)&CRC1)[0] = ((char *)&pFifoBlock->EwfChunkCRC)[3];
      ((char *)&CRC1)[1] = ((char *)&pFifoBlock->EwfChunkCRC)[2];
      ((char *)&CRC1)[2] = ((char *)&pFifoBlock->EwfChunkCRC)[1];
      ((char *)&CRC1)[3] = ((char *)&pFifoBlock->EwfChunkCRC)[0];
   }
   else
   {
      pBufferUncompressed = pFifoBlock->Buffer;
      UncompressedSize = pFifoBlock->DataSize;
      CRC1 = pFifoBlock->EwfChunkCRC;
   }

   // Get the CRC
   // -----------
   CRC2 = adler32 (1, (Bytef*)pBufferUncompressed, UncompressedSize);
   Error = Error || (CRC1 != CRC2);

   // Clean up
   // --------
   if (pFifoBlock->EwfCompressionUsed)
   {
      UTIL_MEM_FREE (pBufferUncompressed);  //lint !e673 Possibly inappropriate deallocation
   }

   if (Error)
   {
      LOG_ERROR ("zrc=%d CRC1=%08X CRC2=%08X Size1=%d Size2=%lu EwfPreprocessed=%d, EwfCompressionUsed=%d EwfWriteCRC=%d ",
                  zrc, CRC1, CRC2, pFifoBlock->DataSize, UncompressedSize,
                                   pFifoBlock->EwfPreprocessed?1:0,
                                   pFifoBlock->EwfCompressionUsed,
                                   pFifoBlock->EwfWriteCRC)
   }
   return NO_ERROR;
}

//lint -save -esym(613,pOutputFile)   Possible use of null pointer
void t_ThreadWrite::run (void)
{
   t_pAcquisition pAcquisition;
   t_pDevice      pDevice;
   t_pFifo        pFifo;
   t_pFifoBlock   pFifoBlock;
   t_OutputFile  *pOutputFile= nullptr;
   bool            Finished  = false;
   bool            FileHandleReleased;
   quint64         Blocks    = 0;
   quint64         Written   = 0;
   APIRET          rc;

   pAcquisition = pOwn->pAcquisition;
   pDevice      = pAcquisition->pDevice;
   pFifo        = *(pOwn->ppFifo);

   LOG_INFO ("[%s] Write thread %d started", QSTR_TO_PSZ (pDevice->LinuxDevice), pAcquisition->Instance)
   CHK_EXIT (SetDebugMessage ("Start run function"))

   pAcquisition->CurrentWritePos.Zero();

   CHK_EXIT (DeleteImageFiles (false))

   switch (pAcquisition->Format)
   {
      case t_File::EWF : if (CONFIG(EwfFormat) == t_File::AEWF)
                              pOutputFile = new t_OutputFileAEWF(this);
                         else 
                              #if (ENABLE_LIBEWF)
                                 pOutputFile = new t_OutputFileEWF (this);
                              #else
                                 CHK_EXIT (ERROR_THREADWRITE_INVALID_FORMAT)
                              #endif 
                         break;
      case t_File::DD  :      pOutputFile = new t_OutputFileDD  (this); break;
      case t_File::AAFF:      pOutputFile = new t_OutputFileAAFF(this); break;
      default: CHK_EXIT (ERROR_THREADWRITE_INVALID_FORMAT)
   }

   rc = pOutputFile->Open (pAcquisition, false);
   if (rc == ERROR_THREADWRITE_OPEN_FAILED)
   {
      LOG_INFO ("[%s] Could not open destination file %s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ (pAcquisition->ImageFilename))
      pDevice->Error.Set(t_Device::t_Error::WriteError, pAcquisition->Instance);
   }
   else
   {
      CHK_EXIT (rc)
   }

   pOwn->SemHandle.lock ();
   pOwn->pOutputFile = pOutputFile;  // Only set pOwn->pOutputFile here, after initialisation completed successfully. The reason is,
   pOwn->SemHandle.unlock ();        // that other threads should remain blocked in t_ThreadWrite::GetpFileHandle until this point.

   while (!Finished && !pDevice->Error.Abort())       // The main loop continues until the very end...
   {
      if (*(pOwn->pSlowDownRequest))
         msleep (THREADWRITE_SLOWDOWN_SLEEP);

      CHK_EXIT (SetDebugMessage ("Wait for block"))
      CHK_EXIT (pFifo->Get (pFifoBlock))
      CHK_EXIT (SetDebugMessage ("Process block"))
      if (pFifoBlock)
      {
//         t_Fifo::LogBlock (pDevice->pFifoMemory, pFifoBlock, 'w');
         if (pFifoBlock->Nr != Blocks)
         {
            LOG_ERROR ("[%s] Fifo block number out of sequence. Expected: %lld Received: %lld", QSTR_TO_PSZ (pDevice->LinuxDevice), Blocks, pFifoBlock->Nr)
//            t_Fifo::LogBlock (pDevice->pFifoMemory, pFifoBlock, 'e');
            CHK_EXIT (ERROR_THREADWRITE_OUT_OF_SEQUENCE_BLOCK)
         }
         Blocks++;
         if (!pDevice->Error.Abort(pAcquisition->Instance))  //  ..., however, if we had an error in this write thread, we only keep
         {                                                   // getting and destroying blocks, without processing them.
            rc = pOwn->pOutputFile->Write (pFifoBlock);

            pAcquisition->CurrentWritePos.Inc (pFifoBlock->DataSize);
            if ((rc == ERROR_THREADWRITE_WRITE_FAILED) ||
                (rc == ERROR_THREADWRITE_OPEN_FAILED))
            {
               const char *pAction = (rc == ERROR_THREADWRITE_WRITE_FAILED) ? "write to" : "open";
               LOG_ERROR ("[%s] Could not %s destination file %s", QSTR_TO_PSZ (pDevice->LinuxDevice), pAction, QSTR_TO_PSZ (pAcquisition->ImageFilename))
               LOG_INFO  ("[%s] Last block sizes: %d - %d - %zd ", QSTR_TO_PSZ (pDevice->LinuxDevice), pFifoBlock->BufferSize, pFifoBlock->DataSize, pFifoBlock->EwfDataSize)
               pDevice->Error.Set(t_Device::t_Error::WriteError, pAcquisition->Instance);
            }
            else
            {
               CHK_EXIT (rc)
            }
            if (pDevice->HasCompressionThreads() && (pAcquisition->Format == t_File::EWF) && CONFIG(CheckEwfData))
               CHK_EXIT (ThreadWriteDebugCheckLibewfData (pFifoBlock))
         }
         Written += pFifoBlock->DataSize;
         Finished = (Written >= pDevice->Size);

         CHK_EXIT (t_Fifo::Destroy (pDevice->pFifoMemory, pFifoBlock))
      }
      else
      {
         LOG_INFO ("[%s] Dummy block", QSTR_TO_PSZ (pDevice->LinuxDevice))
         Finished = true;
      }
   }

   // Wait until file handle released and close it
   // --------------------------------------------
   LOG_INFO ("[%s] Waiting for all other threads using the file handle to finish", QSTR_TO_PSZ (pDevice->LinuxDevice))
   do
   {
      bool AskedForRelease = false;

      pOwn->SemHandle.lock();
      FileHandleReleased = (pOwn->FileHandleRequests == 0);
      pOwn->SemHandle.unlock();
      if (!FileHandleReleased)
      {
         if (!AskedForRelease)
         {
            emit SignalFreeMyHandle (pDevice);
            AskedForRelease = true;
         }
         msleep (THREADWRITE_WAIT_FOR_HANDLE_GRANULARITY);
      }
   } while (!FileHandleReleased);

   LOG_INFO ("[%s] Closing output file", QSTR_TO_PSZ (pDevice->LinuxDevice))
   if (pOwn->pOutputFile->Opened())
   {
      rc = pOwn->pOutputFile->Close ();
      if (rc == ERROR_THREADWRITE_CLOSE_FAILED)
         pDevice->Error.Set(t_Device::t_Error::WriteError, pAcquisition->Instance);
   }

   // Image verification
   // ------------------
   if (pAcquisition->VerifyDst && !pDevice->Error.Abort(pAcquisition->Instance))
   {
      t_HashContextMD5      HashContextMD5;
      t_HashContextSHA1     HashContextSHA1;
      t_HashContextSHA256   HashContextSHA256;
      t_pHashContextMD5    pHashContextMD5    = &HashContextMD5;
      t_pHashContextSHA1   pHashContextSHA1   = &HashContextSHA1;
      t_pHashContextSHA256 pHashContextSHA256 = &HashContextSHA256;
      quint64               Pos = 0;

      if (pAcquisition->CalcMD5)    CHK_EXIT (HashMD5Init    (pHashContextMD5   ))  else pHashContextMD5    = nullptr;
      if (pAcquisition->CalcSHA1)   CHK_EXIT (HashSHA1Init   (pHashContextSHA1  ))  else pHashContextSHA1   = nullptr;
      if (pAcquisition->CalcSHA256) CHK_EXIT (HashSHA256Init (pHashContextSHA256))  else pHashContextSHA256 = nullptr;

      LOG_INFO ("[%s] Reopening image file for verification", QSTR_TO_PSZ (pDevice->LinuxDevice))
      CHK_EXIT (SetDebugMessage ("pOutputFile->Open verification"))
      rc = pOutputFile->Open (pAcquisition, true);
      CHK_EXIT (SetDebugMessage ("Returning from pOutputFile->Open"))
      if (rc == ERROR_THREADWRITE_OPEN_FAILED)
      {
         LOG_INFO ("[%s] Could not open image file %s for verification", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ (pAcquisition->ImageFilename))
         pDevice->Error.Set(t_Device::t_Error::VerifyError, pAcquisition->Instance);
      }
      else
      {
         CHK_EXIT (rc)
      }

      Finished = false;
      while (!Finished && !pDevice->Error.Abort(pAcquisition->Instance))
      {
         rc = pOwn->pOutputFile->Verify (pHashContextMD5, pHashContextSHA1, pHashContextSHA256, pAcquisition->ImageFileHashList, &Pos);
         if (rc == ERROR_THREADWRITE_VERIFY_FAILED)
         {
            LOG_ERROR ("[%s] Could not verify image %s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ (pAcquisition->ImageFilename))
            pDevice->Error.Set(t_Device::t_Error::VerifyError, pAcquisition->Instance);
         }
         else
         {
            CHK_EXIT (rc)
            pAcquisition->CurrentVerifyPosDst.Set(Pos);
            Finished = (Pos >= pDevice->Size);
            if (Finished)
            {
               if (pAcquisition->CalcMD5)    CHK_EXIT (HashMD5Digest    (pHashContextMD5   , &pAcquisition->MD5DigestVerifyDst   ))
               if (pAcquisition->CalcSHA1)   CHK_EXIT (HashSHA1Digest   (pHashContextSHA1  , &pAcquisition->SHA1DigestVerifyDst  ))
               if (pAcquisition->CalcSHA256) CHK_EXIT (HashSHA256Digest (pHashContextSHA256, &pAcquisition->SHA256DigestVerifyDst))
            }
         }
      }
      if (pDevice->Error.Abort(pAcquisition->Instance))
           LOG_INFO ("[%s] Verification aborted, closing output file", QSTR_TO_PSZ (pDevice->LinuxDevice))
      else LOG_INFO ("[%s] Verification finished, closing output file", QSTR_TO_PSZ (pDevice->LinuxDevice))
      if (pOwn->pOutputFile->Opened())
      {
         rc = pOwn->pOutputFile->Close ();
         if (rc == ERROR_THREADWRITE_CLOSE_FAILED)
            pDevice->Error.Set(t_Device::t_Error::VerifyError, pAcquisition->Instance);
      }
   }

   // Finish
   // ------
   delete pOwn->pOutputFile;
   pOwn->pOutputFile = nullptr;

   if (pDevice->Error.Abort(pAcquisition->Instance) && pDevice->DeleteAfterAbort)
   {
      pDevice->SetState (t_Device::Cleanup);
      CHK_EXIT (DeleteImageFiles (true))
   }

   LOG_INFO ("[%s] Write thread %d exits now (%lld blocks processed, %lld bytes written to output file)", QSTR_TO_PSZ (pDevice->LinuxDevice), pAcquisition->Instance, Blocks, pAcquisition->CurrentWritePos.Get())
   CHK_EXIT (SetDebugMessage ("Exit run function"))
}
//lint -restore

static APIRET DeleteImageFiles0 (t_pDevice pDevice, const QDir &Dir, const QStringList &NameFilter)
{
   QFileInfoList  FileInfoList;
   QFileInfo      FileInfo;
   QString        Info;
   bool           Success;

   FileInfoList = Dir.entryInfoList (NameFilter, QDir::Files, QDir::Name);
   while (!FileInfoList.isEmpty())
   {
      FileInfo = FileInfoList.takeFirst();
      CHK (pDevice->SetMessage (QObject::tr("Deleting %1") .arg(FileInfo.fileName())))

      Info = "Deleting " + FileInfo.absoluteFilePath() + " - ";
      Success = QFile::remove (FileInfo.absoluteFilePath());
      if (Success)
           Info += "successful";
      else Info += "could not be deleted";
      LOG_INFO ("[%s] %s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ(Info))
   }

   return NO_ERROR;
}

APIRET t_ThreadWrite::DeleteImageFiles (bool AlsoDeleteInfoFile)
{
   t_pAcquisition pAcquisition = pOwn->pAcquisition;
   t_pDevice      pDevice      = pAcquisition->pDevice;
   QDir            DirImage (pAcquisition->ImagePath);
   QDir            DirInfo  (pAcquisition->InfoPath );
   QString         ExtensionImage;

   if (!pAcquisition->Clone)
   {
      LOG_INFO ("[%s] Deleting existing image files of the same name", QSTR_TO_PSZ (pDevice->LinuxDevice))
      CHK (t_File::GetFormatExtension (pDevice, &ExtensionImage))
      CHK (DeleteImageFiles0 (pDevice, DirImage, QStringList(pAcquisition->ImageFilename + ExtensionImage)))
   }
   if (AlsoDeleteInfoFile)
   {
      LOG_INFO ("[%s] Deleting existing info file of the same name", QSTR_TO_PSZ (pDevice->LinuxDevice))
      CHK (DeleteImageFiles0 (pDevice, DirImage, QStringList(pAcquisition->InfoFilename + t_File::pExtensionInfo)))
   }
   CHK (pDevice->SetMessage (QString()))

   return NO_ERROR;
}

void t_ThreadWrite::SlotFinished (void)
{
   emit SignalEnded (pOwn->pAcquisition->pDevice);
}

APIRET t_ThreadWrite::GetpFileHandle0 (void **ppHandle)
{
   *ppHandle = nullptr;

   if (!isRunning())       return ERROR_THREADWRITE_HANDLE_NOT_YET_AVAILABLE;   // This never happens, as the threads are all created together and only started after that
   if (!pOwn->pOutputFile) return ERROR_THREADWRITE_HANDLE_NOT_YET_AVAILABLE;

   *ppHandle = pOwn->pOutputFile->GetFileHandle();
   if (!*ppHandle)         return ERROR_THREADWRITE_HANDLE_NOT_YET_AVAILABLE;

   return NO_ERROR;
}

APIRET t_ThreadWrite::GetpFileHandle (void **ppHandle)
{
   unsigned int    Wait=0;
   APIRET          rc;
   t_pAcquisition pAcquisition = pOwn->pAcquisition;

   *ppHandle = nullptr;
   do
   {
      if (pAcquisition->pDevice->Error.Abort(pAcquisition->Instance))  // May happen, for example, if the write thread wasn't able to open the destination file.
         break;

      pOwn->SemHandle.lock();
      rc = GetpFileHandle0 (ppHandle);
      if ((rc != ERROR_THREADWRITE_HANDLE_NOT_YET_AVAILABLE) && (rc != NO_ERROR))
      {
         pOwn->SemHandle.unlock();
         CHK (rc)
      }
      if (*ppHandle == nullptr)
      {
         pOwn->SemHandle.unlock();

         Wait += THREADWRITE_WAIT_FOR_HANDLE_GRANULARITY;
         if (Wait > THREADWRITE_WAIT_FOR_HANDLE)
            CHK_EXIT (ERROR_THREADWRITE_HANDLE_TIMEOUT)

         msleep (THREADWRITE_WAIT_FOR_HANDLE_GRANULARITY);
      }
   } while (*ppHandle == nullptr);

   pOwn->FileHandleRequests++;
   pOwn->SemHandle.unlock();

   return NO_ERROR;
}

APIRET t_ThreadWrite::ReleaseFileHandle (void)
{
   pOwn->SemHandle.lock();
   pOwn->FileHandleRequests--;
   pOwn->SemHandle.unlock();

   return NO_ERROR;
}

int t_ThreadWrite::Instance (void)
{
   return pOwn->pAcquisition->Instance;
}
