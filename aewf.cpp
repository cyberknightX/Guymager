// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Multithreaded AEWF (AEWF = Advanced EWF)
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

#include <netinet/in.h>
#include <string.h>
#include <time.h>    //lint !e537 repeated include file
#include <stdlib.h>
#include <errno.h>
#include <zlib.h>
#include <QString>
#include <QStringList>
#include <QtEndian>

#include "ewf.h"
#include "util.h"
#include "config.h"
#include "aewf.h"


// ------------------
//         Qt
// ------------------

// Qt inconsistently defines its little/big endian functions, see QTBUG-21208 on bugreports.qt.nokia.com
// The following lines implement conversin for uchar and avoid the problems.

#if (QT_VERSION < 0x040800)
   template <typename T> T qbswap(T source);
   template <> inline uchar qbswap<uchar>(uchar source)
   {
      return source;
   }
#endif

// ------------------
//  AEWF definitions
// ------------------

static const uchar  AEWF_SIGNATURE[8]  = {0x45, 0x56, 0x46, 0x09, 0x0D, 0x0A, 0xFF, 0x00};
static const uchar  AEWF_STARTOFFIELDS = 0x01;
static const ushort AEWF_ENDOFFIELDS   = 0x0000;

static const char * AEWF_SECTIONNAME_HEADER2 = "header2";
static const char * AEWF_SECTIONNAME_HEADER  = "header" ;
static const char * AEWF_SECTIONNAME_VOLUME  = "volume" ;
static const char * AEWF_SECTIONNAME_SECTORS = "sectors";
static const char * AEWF_SECTIONNAME_TABLE   = "table"  ;
static const char * AEWF_SECTIONNAME_TABLE2  = "table2" ;
static const char * AEWF_SECTIONNAME_DATA    = "data"   ;
static const char * AEWF_SECTIONNAME_ERROR2  = "error2" ;
static const char * AEWF_SECTIONNAME_HASH    = "hash"   ;
static const char * AEWF_SECTIONNAME_DIGEST  = "digest" ;
static const char * AEWF_SECTIONNAME_NEXT    = "next"   ;
static const char * AEWF_SECTIONNAME_DONE    = "done"   ;

static const unsigned int AEWF_MAX_CHUNK_OFFSETS      = 65534;
static const unsigned int AEWF_MAX_SIZE_SECTIONSECTOR = 0x7FFFFFFF;

static const unsigned int AEWF_COMPRESSED_CHUNK = 0x80000000;

static const unsigned long long AEWF_SEEK_END = 0xFFFFFFFFFFFFFFFFULL;

typedef struct
{
   uchar  Signature[8];
   uchar  StartOfFields;
   ushort SegmentNumber;
   ushort EndOfFields;
} __attribute__ ((packed)) t_AewfFileHeader, *t_pAewfFileHeader;

typedef struct
{
   char    Name[16];
   quint64 NextSectionOffset; // based on file start
   quint64 SectionSize;
   uchar   Padding[40];
   uint    CRC;               // CRC of data from beginning of this section to the field before CRC
   uchar   Data[];
} __attribute__ ((packed)) t_AewfSection, *t_pAewfSection;

typedef struct
{
   uchar   MediaType;
   uchar   Unknown1[3];  // contains 0x00
   uint    ChunkCount;
   uint    SectorsPerChunk;
   uint    BytesPerSector;
   quint64 SectorCount;
   uint    CHS_Cylinders;
   uint    CHS_Heads;
   uint    CHS_Sectors;
   uchar   MediaFlags;
   uchar   Unknown2[3];  // contains 0x00
   uint    PalmVolumeStartSector;
   uchar   Padding1[4];  // contains 0x00
   uint    SmartLogsStartSector;
   uchar   CompressionLevel;
   uchar   Unknown3[3];  // contains 0x00
   uint    ErrorBlockSize;
   uchar   Unknown4[4];
   uchar   AcquirySystemGUID[16];
   uchar   Padding2[963];
   uchar   Reserved [5];
   uint    Checksum;
} __attribute__ ((packed)) t_AewfSectionVolumeContents, *t_pAewfSectionVolumeContents;

typedef struct
{
   uint    Offsets;
   uint    Padding1;
   quint64 BaseOffset;
   uint    Padding2;
   uint    CRC1;
   uint    OffsetArr[0];
   uint    CRC2;          // unused - do not remove or else sizeof calculation become wrong
} __attribute__ ((packed)) t_AewfSectionTableContents;

typedef struct
{
   uint FirstSector;
   uint NumberOfSectors;
} __attribute__ ((packed)) t_AewfSectionErrorEntry;

typedef struct
{
   uint                    NumberOfErrors;
   uchar                   Padding[512];
   uint                    CRC1;
   t_AewfSectionErrorEntry ErrorArr[0];
   uint                    CRC2;     // unused - do not remove or else sizeof calculation become wrong
} __attribute__ ((packed)) t_AewfSectionErrorContents;

typedef struct
{
   uchar MD5    [16];
   uchar Unknown[16];
   uint  CRC;
} __attribute__ ((packed)) t_AewfSectionHashContents;

typedef struct
{
   uchar MD5    [16];
   uchar SHA1   [20];
   uchar Padding[40];
   uint  CRC;
} __attribute__ ((packed)) t_AewfSectionDigestContents;

class t_Aewf
{
   public:
      QString           SegmentFilename;      // The base path and filename, without extension
      quint64           DeviceSize;
      quint64           SegmentSize;
      quint64           ChunkSize;
      quint64           SectorSize;
      t_AewfMediaType   MediaType;
      t_AewfMediaFlags  MediaFlags;
      QString           Description;
      QString           CaseNumber;
      QString           EvidenceNumber;
      QString           Examiner;
      QString           Notes;
      QString           DeviceModel;
      QString           SerialNumber;
      QString           ImagerVersion;
      QString           OSVersion;

      uint              SegmentFiles;
//      uchar             EwfCompressionLevel;   // The compression levels have been made global in order not to depend upon
//      int               ZipCompressionLevel;   // Aewf handle in preprocessing (this simplifies duplicate image writing)
      QStringList       SegmentFilenameList;
      FILE            *pCurrentSegmentFile;
      quint64           CurrentSectionSectorSeekPos;
      quint64           CurrentSectionSectorContentSize;
      t_AewfFileHeader  FileHeaderCache;
      quint64           ChunkBaseOffset;
      uint              ChunkOffsetArr[AEWF_MAX_CHUNK_OFFSETS+1];  // Data is stored in little endian for speed optimisation here! +1 as we add a fake chunk offset marking the end of the section "sectors" when verifying the data
      uint              ChunkOffsets;
      uint              CurrentChunkOffset;           // For knowing the current chunk during verification

      t_HashContextMD5  CurrentSegmentHashContextMD5;  // For segment file MD5 calculation during verification
      uchar           *pVerifyBuffer;                  // For uncompression and other verification tasks
      uint              VerifyBufferLen;

      t_Aewf ()
      {
         pCurrentSegmentFile = nullptr;
         SegmentFiles        = 0;
         pVerifyBuffer       = nullptr;
         VerifyBufferLen     = 0;

         memcpy (FileHeaderCache.Signature, AEWF_SIGNATURE, sizeof (AEWF_SIGNATURE));
         FileHeaderCache.StartOfFields = qToLittleEndian (AEWF_STARTOFFIELDS);
         FileHeaderCache.SegmentNumber = qToLittleEndian (0);
         FileHeaderCache.EndOfFields   = qToLittleEndian (AEWF_ENDOFFIELDS);
//         switch (CONFIG(EwfCompression))
//         {
//            case LIBEWF_COMPRESSION_NONE: ZipCompressionLevel = 0; EwfCompressionLevel = 0x00; break;
//            case LIBEWF_COMPRESSION_FAST: ZipCompressionLevel = 1; EwfCompressionLevel = 0x01; break;
//            case LIBEWF_COMPRESSION_BEST: ZipCompressionLevel = 9; EwfCompressionLevel = 0x02; break;
//            default: CHK_EXIT (ERROR_AEWF_INVALID_COMPRESSION_LEVEL)
//         }
      }
};

// ----------------
//   Global vars
// ----------------
static unsigned char *pAewfZeroBlockCompressed;
static ulong           AewfZeroBlockCompressedLen;
static ulong           AewfZeroBlockUnCompressedLen;

static int             AewfZipCompressionLevel;
static uchar           AewfEwfCompressionLevel;

// ----------------
//  Error handling
// ----------------

#define CHK_FWRITE(Fn)                    \
   if ((Fn) != 1)                         \
      CHK (ERROR_AEWF_CANNOT_WRITE_FILE)

// -------------------
//  Utility functions
// -------------------

static int AewfCRC (const void *pBuffer, size_t Len)
{
   return adler32 (1, (const Bytef *) pBuffer, Len);
}


// EWF file extension calculation: That extension naming convention of EWF files most probably was a quick hack.
// The known sequence is:
//    E01 - E99
//    EAA - EZZ
//    FAA - ZZZ
// Nobody exactly seems to know what follows then. Joachim Metz writes that maybe [AA would follow. If so, it
// doesn't help a lot, as it only doubles the available naming space (the following ASCII character is \ and
// thus cannot be used on Windows systems with their inconsistent usage of \ and / ).
// Guymager suports 2 ways for continuing beyond extension ZZZ:
// OLD method:
//    Here I decided to continue with ZZZxxx, where xxx represents a number from 000 to ZZZ in base36 notation (i.e.
//    0-9 and A-Z). After that, it would continue with ZZZxxxx and so on.
// FTK method
//   According to tests made by Willi Spiegel, FTK continues with E14972, E14973 and so on.
//
// Reminder: Old-fashionned Windows-file systems only allow 3 letters for the extension. So, extensions beyond ZZZ
// must be avoided there.

const QString AewfGetExtension (uint SegmentFileNr)
{
   QString Extension;

   if (SegmentFileNr == 0)
      Extension = "Error_filenr_is_zero";
   else if (SegmentFileNr < 100)
      Extension = QString("E%1").arg(SegmentFileNr,2,10,QChar('0'));
   else
   {
      SegmentFileNr += ('E' -'A') * 26 * 26 - 100; // The first extension after .E99 is .EAA. So, this is the offset we need
      unsigned int DigitL =  SegmentFileNr % 26;
      unsigned int DigitM = (SegmentFileNr / 26) % 26;
      unsigned int DigitH = (SegmentFileNr / (26*26));
      if (DigitH < 26)
      {
         Extension = QString (char (DigitH + 'A')) +
                     QString (char (DigitM + 'A')) +
                     QString (char (DigitL + 'A'));
      }
      else
      {
         switch (CONFIG(EwfNaming))
         {
            case EWFNAMING_OLD:
               SegmentFileNr -= 26*26*26; // substract ZZZ+1 -> so, for the segment following ZZZ the results is 0
               Extension = QString("ZZZ%1").arg(SegmentFileNr,3,36,QChar('0')).toUpper();
               break;
            case EWFNAMING_FTK:
               SegmentFileNr -= 26*26*26; // substract ZZZ+1 -> so, for the segment following ZZZ the results is 0
               SegmentFileNr += 14972;
               Extension = QString("E%1").arg(SegmentFileNr);
               break;
            default:
               CHK_EXIT (ERROR_AEWF_INVALID_EWFNAMING)
         }
      }
   }

   return Extension;
}

// AewfCompress: No need to have an extra check if compression is set to "none". If this is the case, pAewf->ZipCompressionLevel
// is set to 0 and compress2 won't do any compression (see libz / compress2 documentation).
// As well, no need to check for compression level "empty", as AewfZipCompressionLevel is set to 0 in this case (see AewfInit)
static APIRET AewfCompress (void *pDst, ulong *pDstLen, const void* pSrc, unsigned SrcLen)
{
   int rc;

   if ((SrcLen == AewfZeroBlockUnCompressedLen) && UtilIsZero ((unsigned char *)pSrc, SrcLen))  // Check if pre-compressed
   {                                                                                            // zero block can be used
      if (*pDstLen < AewfZeroBlockCompressedLen)
         return ERROR_AEWF_COMPRESSION_FAILED;
      *pDstLen = AewfZeroBlockCompressedLen;
      memcpy (pDst, pAewfZeroBlockCompressed, AewfZeroBlockCompressedLen);
   }
   else
   {
      rc = compress2 ((Bytef*)pDst, pDstLen, (Bytef*)pSrc, SrcLen, AewfZipCompressionLevel);
      if (rc != Z_OK)
         return ERROR_AEWF_COMPRESSION_FAILED;
   }
   return NO_ERROR;
}

static APIRET AewfUncompress (void *pUncompressed, ulong *pUncompressedLen, void *pCompressed, ulong CompressedLen)
{
   int rc;

//   #define AEWF_SIMULATE_IMAGE_FAILURE
   #ifdef AEWF_SIMULATE_IMAGE_FAILURE
      #warning "AEWF_SIMULATE_IMAGE_FAILURE debugging code active"
      static int Debug=0;
      if (Debug++ == 5654)
         return ERROR_AEWF_UNCOMPRESS_FAILED;
   #endif

   if ((CompressedLen == AewfZeroBlockCompressedLen) &&
       (memcmp(pCompressed, pAewfZeroBlockCompressed, CompressedLen) == 0))
   {
      if (*pUncompressedLen < AewfZeroBlockUnCompressedLen)
         return ERROR_AEWF_UNCOMPRESS_ZEROBLOCK_FAILED;
      *pUncompressedLen = AewfZeroBlockUnCompressedLen;
      memset (pUncompressed, 0, AewfZeroBlockUnCompressedLen);
   }
   else
   {
      rc = uncompress ((Bytef*)pUncompressed, pUncompressedLen, (Bytef*)pCompressed, CompressedLen);
      if (rc != Z_OK)
         return ERROR_AEWF_UNCOMPRESS_FAILED;
   }
   return NO_ERROR;
}

static quint64 AewfGetCurrentSeekPos (t_Aewf *pAewf)
{
   if (pAewf->pCurrentSegmentFile)
        return ftello64 (pAewf->pCurrentSegmentFile);
   else return (quint64)(-1);
}

static APIRET AewfSetCurrentSeekPos (t_Aewf *pAewf, quint64 SeekPos)
{
   int rc;

   if (pAewf->pCurrentSegmentFile == nullptr)
      CHK (ERROR_AEWF_NO_FILE_OPEN)

   if (SeekPos == AEWF_SEEK_END)
        rc = fseeko64 (pAewf->pCurrentSegmentFile, 0      , SEEK_END);
   else rc = fseeko64 (pAewf->pCurrentSegmentFile, SeekPos, SEEK_SET);
   if (rc)
      return ERROR_AEWF_CANNOT_SEEK;
   return NO_ERROR;
}

// ---------------------
//  Low level functions
// ---------------------

static inline APIRET AewfWriteFile (t_pAewf pAewf, const void *pBuffer, size_t Len)
{
   if (pAewf->pCurrentSegmentFile == nullptr)
      CHK (ERROR_AEWF_NO_FILE_OPEN)

   CHK_FWRITE (fwrite (pBuffer, Len, 1, pAewf->pCurrentSegmentFile))

   // Simulate cases write error
   // static int FakeWriteError=0;
   // if (FakeWriteError++ == 1500)
   //   CHK (ERROR_AEWF_CANNOT_WRITE_FILE)

   return NO_ERROR;
}

static APIRET AewfCreateFile (t_Aewf *pAewf)
{
   QString Extension;
   QString Filename;

   if (pAewf->pCurrentSegmentFile)
      CHK (ERROR_AEWF_ANOTHER_FILE_STILL_OPEN)

   // Get extension and open file
   // ---------------------------
   pAewf->SegmentFiles++;
   Extension = AewfGetExtension (pAewf->SegmentFiles);
   Filename  = pAewf->SegmentFilename + "." + Extension;

   pAewf->pCurrentSegmentFile = fopen64 (QSTR_TO_PSZ(Filename), "w");
   // Simulate cases open error
   // static int FakeOpenError=0;
   // if (FakeOpenError++ == 2)
   //  pAewf->pCurrentSegmentFile = nullptr;

   if (pAewf->pCurrentSegmentFile == nullptr)
      CHK (ERROR_AEWF_CANNOT_CREATE_FILE)

   pAewf->SegmentFilenameList += Filename;

   // Reset fields related to current segment file
   // --------------------------------------------
   pAewf->ChunkBaseOffset                 = 0;
   pAewf->CurrentSectionSectorSeekPos     = 0;
   pAewf->CurrentSectionSectorContentSize = 0;
   pAewf->ChunkOffsets                    = 0;

   // Write file header
   // -----------------
   pAewf->FileHeaderCache.SegmentNumber = qToLittleEndian ((ushort)pAewf->SegmentFiles);
   CHK (AewfWriteFile (pAewf, &pAewf->FileHeaderCache, sizeof (pAewf->FileHeaderCache)))

   return NO_ERROR;
}

static APIRET AewfCloseFile (t_pAewf pAewf)
{
   FILE *pCurrentSegmentFile = pAewf->pCurrentSegmentFile;
   int    rc, err;

   if (pCurrentSegmentFile == nullptr)
      CHK (ERROR_AEWF_NO_FILE_OPEN)

   pAewf->pCurrentSegmentFile = nullptr;

   rc = fflush (pCurrentSegmentFile);
   if (rc)
   {
      err = errno;
      LOG_ERROR ("fflush returned %d (errno %d - %s)", rc, err, strerror(err));

      (void) UtilClose (pCurrentSegmentFile);
      CHK (ERROR_AEWF_CANNOT_FLUSH_FILE)
   }

   rc = UtilClose (pCurrentSegmentFile);
   if (rc)
   {
      err = errno;
      LOG_ERROR ("UtilClose returned %d (errno %d - %s)", rc, err, strerror(err));
      CHK (ERROR_AEWF_CANNOT_CLOSE_FILE)
   }
// Simulate cases close error
//   static int FakeCloseError=0;
//   if (FakeCloseError++ == 2)
//      CHK (ERROR_AEWF_CANNOT_CLOSE_FILE)

   return NO_ERROR;
}

static APIRET AewfWriteSection (t_pAewf pAewf, const char *pName, const void *pData, quint64 Len)
{
   t_AewfSection Section;

   memset (Section.Padding, 0, sizeof (Section.Padding));
   memset (Section.Name   , 0, sizeof (Section.Name   ));
   strcpy (Section.Name, pName);
   if ((pData==nullptr) && (Len==0))
   {
      Section.NextSectionOffset = qToLittleEndian (AewfGetCurrentSeekPos(pAewf)); // Sections "next" and "done", the only ones without
      Section.SectionSize       = 0;                                              // additional data, refer to themselves and have size set to 0
   }
   else
   {
      Section.NextSectionOffset = qToLittleEndian (AewfGetCurrentSeekPos(pAewf) + sizeof (t_AewfSection) + Len);
      Section.SectionSize       = qToLittleEndian (sizeof (t_AewfSection) + Len);
   }
   Section.CRC = qToLittleEndian (AewfCRC (&Section, offsetof (t_AewfSection, CRC)));

   CHK (AewfWriteFile (pAewf, &Section, sizeof (t_AewfSection)))
   if (pData && Len)
      CHK (AewfWriteFile (pAewf, pData, Len))

   return NO_ERROR;
}

static APIRET AewfWriteSectionHeader (t_pAewf pAewf)  // Writes the sections header2, header2 and header alltogether
{
   QString        HeaderData;
   time_t         NowT;
   int            Utf16Codes, i;
   ushort       *pArr;
   ulong          ArrLen;
   void         *pCompressed;
   ulong          CompressedLen;
   const ushort *pUtf16;
   APIRET         rc;
   QByteArray     ByteArr;
   QString        TimeBuff;
   struct tm      TM;

   time (&NowT);
   localtime_r (&NowT, &TM);

   // Generate 2 sections "header2"
   // -----------------------------
   HeaderData  = QString ("3\nmain\n");                                    // 1 2
   HeaderData += QString ("a\tc\tn\te\tt\tmd\tsn\tav\tov\tm\tu\tp\tdc\n"); // 3
   HeaderData += QString ("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t%10\t%11\t\t\n") .arg(pAewf->Description)  .arg(pAewf->CaseNumber)    .arg(pAewf->EvidenceNumber)
                                                                                .arg(pAewf->Examiner)     .arg(pAewf->Notes)         .arg(pAewf->DeviceModel)
                                                                                .arg(pAewf->SerialNumber) .arg(pAewf->ImagerVersion) .arg(pAewf->OSVersion)
                                                                                .arg(NowT)                .arg(NowT);    // 4
   HeaderData += QString ("\nsrce\n0\t1\n");                               // 5 6 7
   HeaderData += QString ("p\tn\tid\tev\ttb\tlo\tpo\tah\tgu\taq\n");       // 8
   HeaderData += QString ("0\t0\n");                                       // 9
   HeaderData += QString ("\t\t\t\t\t-1\t-1\t\t\t\n");                     // 10
   HeaderData += QString ("\nsub\n0\t1\n");                                // 11 12 13
   HeaderData += QString ("p\tn\tid\tnu\tco\tgu\n");                       // 14
   HeaderData += QString ("0\t0\n\t\t\t\t1\t\n");                          // 15 16

   pUtf16 = HeaderData.utf16();
   Utf16Codes=0;
   while (pUtf16[Utf16Codes] != 0)
      Utf16Codes++;

   ArrLen = (Utf16Codes+1)*sizeof(ushort);
   pArr = (ushort *) UTIL_MEM_ALLOC (ArrLen);  // +1 for BOM
   pArr[0] = qToLittleEndian((ushort)0xFEFF);  // BOM
   for (i=0; i<Utf16Codes; i++)
      pArr[i+1] = qToLittleEndian(pUtf16[i]);

   CompressedLen = UtilGetMaxZcompressedBufferSize (ArrLen);
   pCompressed   = UTIL_MEM_ALLOC (CompressedLen);
   CHK (AewfCompress (pCompressed, &CompressedLen, pArr, ArrLen))

   rc = AewfWriteSection (pAewf, AEWF_SECTIONNAME_HEADER2, pCompressed, CompressedLen);
   rc = AewfWriteSection (pAewf, AEWF_SECTIONNAME_HEADER2, pCompressed, CompressedLen);
   UTIL_MEM_FREE (pArr);
   UTIL_MEM_FREE (pCompressed);
   CHK (rc)

   // Generate section "header"
   // -------------------------
   TimeBuff = QString ("%1 %2 %3 %4 %5 %6") .arg (TM.tm_year+1900) .arg (TM.tm_mon+1) .arg (TM.tm_mday) .arg (TM.tm_hour) .arg (TM.tm_min) .arg (TM.tm_sec);

   HeaderData  = QString ("1\nmain\n");                                    // 1 2
   HeaderData += QString ("c\tn\ta\te\tt\tav\tov\tm\tu\tp\n");             // 3
   HeaderData += QString ("%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\t0\n\n") .arg(pAewf->CaseNumber) .arg(pAewf->EvidenceNumber) .arg(pAewf->Description)
                                                                       .arg(pAewf->Examiner)   .arg(pAewf->Notes)          .arg(pAewf->ImagerVersion)
                                                                       .arg(pAewf->OSVersion)  .arg(TimeBuff)              .arg(TimeBuff);    // 4

   ByteArr = HeaderData.toLatin1();
   CompressedLen = UtilGetMaxZcompressedBufferSize (ByteArr.length());
   pCompressed   = UTIL_MEM_ALLOC (CompressedLen);
   CHK (AewfCompress (pCompressed, &CompressedLen, ByteArr.constData(), ByteArr.length()))

   rc = AewfWriteSection (pAewf, AEWF_SECTIONNAME_HEADER, pCompressed, CompressedLen);
   UTIL_MEM_FREE (pCompressed);
   CHK (rc)

   return NO_ERROR;
}

static APIRET AewfWriteSectionVolume (t_Aewf *pAewf, bool NameVolume=true)
{
   t_AewfSectionVolumeContents VolumeContents;
   quint64                     Chunks;

   Chunks = pAewf->DeviceSize / pAewf->ChunkSize;
   if (pAewf->DeviceSize % pAewf->ChunkSize)
      Chunks++;

   memset (&VolumeContents, 0, sizeof(VolumeContents));
   VolumeContents.MediaType        = qToLittleEndian ((uchar)pAewf->MediaType);
   VolumeContents.ChunkCount       = qToLittleEndian ((uint)    Chunks);
   VolumeContents.SectorsPerChunk  = qToLittleEndian ((uint)   (pAewf->ChunkSize / pAewf->SectorSize));
   VolumeContents.BytesPerSector   = qToLittleEndian ((uint)    pAewf->SectorSize);
   VolumeContents.SectorCount      = qToLittleEndian ((quint64) pAewf->DeviceSize / pAewf->SectorSize);
   VolumeContents.MediaFlags       = qToLittleEndian ((uchar)  (pAewf->MediaFlags | AEWF_MEDIAFLAGS_IMAGE));
   VolumeContents.CompressionLevel = qToLittleEndian ((uchar)   AewfEwfCompressionLevel);
   VolumeContents.ErrorBlockSize   = qToLittleEndian ((uint)    1);
   VolumeContents.Checksum         = qToLittleEndian (AewfCRC (&VolumeContents, offsetof(t_AewfSectionVolumeContents, Checksum)));
   if (NameVolume)
        CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_VOLUME, &VolumeContents, sizeof(VolumeContents)))
   else CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_DATA  , &VolumeContents, sizeof(VolumeContents)))

   return NO_ERROR;
}

static APIRET AewfFinishSectionSectors (t_Aewf *pAewf)
{
   CHK (AewfSetCurrentSeekPos (pAewf, pAewf->CurrentSectionSectorSeekPos))
   CHK (AewfWriteSection      (pAewf, AEWF_SECTIONNAME_SECTORS, nullptr, pAewf->CurrentSectionSectorContentSize))
   CHK (AewfSetCurrentSeekPos (pAewf, AEWF_SEEK_END))

   pAewf->CurrentSectionSectorSeekPos     = 0;
   pAewf->CurrentSectionSectorContentSize = 0;

   return NO_ERROR;
}

static APIRET AewfWriteSectionTable (t_Aewf *pAewf)
{
   t_AewfSectionTableContents *pTableContents;
   uint                         TableContentsSize;
   quint64                      OffsetArrSize;
   uint                       *pCRC2;

   OffsetArrSize = pAewf->ChunkOffsets * sizeof (unsigned int);
   TableContentsSize = sizeof (t_AewfSectionTableContents) + OffsetArrSize;
   pTableContents = (t_AewfSectionTableContents *) UTIL_MEM_ALLOC (TableContentsSize);

   memset (pTableContents, 0, sizeof(t_AewfSectionTableContents));
   pTableContents->Offsets    = qToLittleEndian (pAewf->ChunkOffsets   );
   pTableContents->BaseOffset = qToLittleEndian (pAewf->ChunkBaseOffset);
   pTableContents->CRC1       = qToLittleEndian (AewfCRC(pTableContents, offsetof(t_AewfSectionTableContents, CRC1)));
   memcpy (&pTableContents->OffsetArr[0], &pAewf->ChunkOffsetArr[0], OffsetArrSize);     // pAewf->ChunkOffsetArr already is in little endian!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
   pCRC2 = (unsigned int *) (&pTableContents->OffsetArr[0] + pAewf->ChunkOffsets);
#pragma GCC diagnostic pop
   *pCRC2 = qToLittleEndian (AewfCRC(pTableContents->OffsetArr, OffsetArrSize));

   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_TABLE , pTableContents, sizeof(t_AewfSectionTableContents)+OffsetArrSize))
   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_TABLE2, pTableContents, sizeof(t_AewfSectionTableContents)+OffsetArrSize))
   UTIL_MEM_FREE (pTableContents);

   memset (pAewf->ChunkOffsetArr, 0, sizeof(pAewf->ChunkOffsetArr));
   pAewf->ChunkOffsets    = 0;
   pAewf->ChunkBaseOffset = 0;

   return NO_ERROR;
}

static APIRET AewfWriteSectionError (t_Aewf *pAewf, QList<quint64> &BadSectors)
{
   t_AewfSectionErrorContents *pErrorContents;
   uint                       *pCRC2;
   uint                         Entries;
   uint                         SectionSize;
   quint64                      i, Count;
   quint64                      From, To, Next;

   Count = BadSectors.count();
   if (Count == 0)
      return NO_ERROR;

   pErrorContents = (t_AewfSectionErrorContents *) UTIL_MEM_ALLOC (sizeof(t_AewfSectionErrorContents));
   memset (pErrorContents, 0, sizeof(t_AewfSectionErrorContents));
   Entries   = 0;
   i         = 0;
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
      Entries++;
      SectionSize = sizeof(t_AewfSectionErrorContents) + Entries * sizeof (t_AewfSectionErrorEntry);
      pErrorContents = (t_AewfSectionErrorContents *) realloc (pErrorContents, SectionSize);
      pErrorContents->ErrorArr[Entries-1].FirstSector     = qToLittleEndian (From);
      pErrorContents->ErrorArr[Entries-1].NumberOfSectors = qToLittleEndian (To - From + 1);
   }
   pCRC2 = (unsigned int *) &pErrorContents->ErrorArr[Entries];
   pErrorContents->NumberOfErrors = qToLittleEndian (Entries);
   pErrorContents->CRC1           = qToLittleEndian (AewfCRC (pErrorContents, offsetof (t_AewfSectionErrorContents, CRC1)));
   *pCRC2                         = qToLittleEndian (AewfCRC (&pErrorContents->ErrorArr[0], Entries * sizeof (t_AewfSectionErrorEntry)));

   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_ERROR2, pErrorContents, SectionSize))

   return NO_ERROR;
}

static APIRET AewfWriteSectionHash (t_Aewf *pAewf, const uchar *pMD5)
{
   t_AewfSectionHashContents HashContents;

   memset (&HashContents, 0, sizeof(HashContents));
   memcpy (HashContents.MD5, pMD5, sizeof(HashContents.MD5));
   HashContents.CRC = qToLittleEndian (AewfCRC (&HashContents, offsetof (t_AewfSectionHashContents, CRC)));

   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_HASH, &HashContents, sizeof(HashContents)))
   return NO_ERROR;
}

static APIRET AewfWriteSectionDigest (t_Aewf *pAewf, const uchar *pMD5, const uchar *pSHA1)
{
   t_AewfSectionDigestContents DigestContents;

   memset (&DigestContents, 0, sizeof(DigestContents));
   memcpy (DigestContents.MD5 , pMD5 , sizeof(DigestContents.MD5 ));
   memcpy (DigestContents.SHA1, pSHA1, sizeof(DigestContents.SHA1));
   DigestContents.CRC = qToLittleEndian (AewfCRC (&DigestContents, offsetof (t_AewfSectionDigestContents, CRC)));

   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_DIGEST, &DigestContents, sizeof(DigestContents)))
   return NO_ERROR;
}


static APIRET AewfWriteSectionNext (t_Aewf *pAewf)
{
   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_NEXT, nullptr, 0))
   return NO_ERROR;
}

static APIRET AewfWriteSectionDone (t_Aewf *pAewf)
{
   CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_DONE, nullptr, 0))
   return NO_ERROR;
}

// ---------------------
//      API functions
// ---------------------

APIRET AewfOpen (t_pAewf *ppAewf, const char *pFilename,
                                  unsigned long long DeviceSize, unsigned long long SegmentSize,
                                  unsigned int       ChunkSize , unsigned int       SectorSize,
                                  t_AewfMediaType    MediaType,
                                  t_AewfMediaFlags   MediaFlags,
                                  QString Description, QString CaseNumber,  QString EvidenceNumber, QString Examiner,
                                  QString Notes,       QString DeviceModel, QString SerialNumber,   QString ImagerVersion,
                                  QString OSVersion)
{
   *ppAewf = new t_Aewf;
   (*ppAewf)->SegmentFilename = pFilename;
   (*ppAewf)->DeviceSize      = DeviceSize;
   (*ppAewf)->SegmentSize     = SegmentSize;
   (*ppAewf)->ChunkSize       = ChunkSize;
   (*ppAewf)->SectorSize      = SectorSize;
   (*ppAewf)->MediaType       = MediaType;
   (*ppAewf)->MediaFlags      = MediaFlags;
   (*ppAewf)->Description     = Description;
   (*ppAewf)->CaseNumber      = CaseNumber;
   (*ppAewf)->EvidenceNumber  = EvidenceNumber;
   (*ppAewf)->Examiner        = Examiner;
   (*ppAewf)->Notes           = Notes;
   (*ppAewf)->DeviceModel     = DeviceModel;
   (*ppAewf)->SerialNumber    = SerialNumber;
   (*ppAewf)->ImagerVersion   = ImagerVersion;
   (*ppAewf)->OSVersion       = OSVersion;

   CHK (AewfCreateFile         (*ppAewf))
   CHK (AewfWriteSectionHeader (*ppAewf))
   CHK (AewfWriteSectionVolume (*ppAewf))

   return NO_ERROR;
}


APIRET AewfPreprocess (t_pAewfPreprocess pPreprocess, uchar *pDataIn, unsigned int DataLenIn, uchar *pDataOut, unsigned int DataLenOut)
{
//   t_pAewfPreprocess pPreProcess;
   int                rc;
   uLongf             LenOut;
   uint             *pCRC;
   uint               MaxLenOut;
#ifdef AEWF_COMPRESSION_DEBUG
   static bool        FirstCall=true;
   static int         CharCount=0;
#endif
//   *ppPreprocess = nullptr;
//   pPreProcess = (t_pAewfPreprocess) UTIL_MEM_ALLOC(sizeof(t_AewfPreprocess));
//   if (pPreProcess == nullptr)
//      CHK (ERROR_AEWF_MEMALLOC_FAILED)
//   *ppPreprocess = pPreProcess;
   pPreprocess->Compressed = false;

   if (CONFIG(EwfCompression) != LIBEWF_COMPRESSION_NONE)
   {
      LenOut = DataLenOut;
      rc = AewfCompress (pDataOut, &LenOut, pDataIn, DataLenIn);

      pPreprocess->DataLenOut = LenOut;
      MaxLenOut = (unsigned int) (DataLenIn * CONFIG(EwfCompressionThreshold));
      MaxLenOut = (unsigned int) GETMIN (DataLenIn-1, MaxLenOut); // Avoid potential float operation side effects
#ifdef AEWF_COMPRESSION_DEBUG
      if (FirstCall)
      {
         printf ("\nAEWF -- Chunk size:                      %u"                                      , DataLenIn);
         printf ("\nAEWF -- Min. required compression ratio: %1.3f (EwfCompressionThreshold)"         , CONFIG(EwfCompressionThreshold));
         printf ("\nAEWF -- Max. compressed size:            %u (EwfCompressionThreshold * ChunkSize)", MaxLenOut);
         printf ("\nAEWF -- List of chunks (. = not compressed because compressed data is larger than uncompressed data,\n"
                   "                        M = not compressed because compressed data is larger max. compressed size,\n"
                   "                        c = Compressed\n");
         FirstCall = false;
      }
#endif
      if (rc != NO_ERROR)
           LOG_ERROR ("compress2 returned %d - treating this block as uncompressed in order to resume", rc)
      else pPreprocess->Compressed = (LenOut <= MaxLenOut);
   }

   if (!pPreprocess->Compressed)  // Add the CRC to the end of the non-compressed data. The FIFO buffer is
   {                              // big enough for this (see extra space calculation)
      pCRC = (unsigned int *) &pDataIn[DataLenIn];
      *pCRC = qToLittleEndian (AewfCRC (pDataIn, DataLenIn));
      pPreprocess->DataLenOut = DataLenIn + sizeof (unsigned int);
#ifdef AEWF_COMPRESSION_DEBUG
      if (LenOut < DataLenIn)
           printf("M");
      else printf(".");
#endif
   }
#ifdef AEWF_COMPRESSION_DEBUG
   else
   {
      printf("c");
   }

   if (++CharCount == 80)
   {
      printf ("\n");
      CharCount=0;
   }
#endif
   return NO_ERROR;
}

APIRET AewfWrite (t_Aewf *pAewf, t_AewfPreprocess *pPreprocess, const uchar *pData, unsigned int DataLen)
{
   quint64 SectionTableSize;
   quint64 MinRemainingSize;
   uint    OffsetArrEntry;
   uint    NewLen;

   if (pAewf->pCurrentSegmentFile == nullptr)
      CHK (ERROR_AEWF_NO_FILE_OPEN)

//   printf ("\nChunk: %d  %u bytes - %s", pAewf->ChunkOffsets, DataLen, pPreprocess->Compressed ? "compressed" : "uc");

   // Check if enough space remaining in current segment file
   // -------------------------------------------------------
   SectionTableSize = sizeof (t_AewfSection) + sizeof (t_AewfSectionTableContents)
                    + (pAewf->ChunkOffsets + 1)* sizeof(unsigned int);
   MinRemainingSize =     DataLen
                    + 2 * SectionTableSize                                              // Sections "table" and "table2"
                    +     sizeof (t_AewfSection) + sizeof (t_AewfSectionVolumeContents) // Section "data"
                    +     sizeof (t_AewfSection) + sizeof (t_AewfSectionHashContents  ) // Section "hash"
                    +     sizeof (t_AewfSection) + sizeof (t_AewfSectionDigestContents) // Section "digest"
                    +     sizeof (t_AewfSection)                                        // Section "done"
                    +     sizeof (t_AewfSection) +                                      // In case we need to start a new section "sectors" (see below),
                    + 2 * sizeof (t_AewfSection) + sizeof (t_AewfSectionTableContents); // which would also require additonal sections "table" and "table2"
   if ((pAewf->SegmentSize - AewfGetCurrentSeekPos(pAewf)) < MinRemainingSize)
   {
      // Close the current segment and open a new one
      CHK (AewfFinishSectionSectors(pAewf))
      CHK (AewfWriteSectionTable   (pAewf))
      CHK (AewfWriteSectionNext    (pAewf))
      CHK (AewfCloseFile           (pAewf))
      CHK (AewfCreateFile          (pAewf))
      CHK (AewfWriteSectionVolume  (pAewf, false))
   }

   // Check if current sector section or offset table is full
   // -------------------------------------------------------
   NewLen = pAewf->CurrentSectionSectorContentSize + DataLen;
   if (pAewf->ChunkOffsets)                                         // We have to add the offset of the first chunk, which normally is 76 (see also comment related to
      NewLen += pAewf->ChunkOffsetArr[0] & ~AEWF_COMPRESSED_CHUNK;  // ewfacquire behaviour below). However, we only know this for sure after a first chunk has been written.
                                                                    // This is no problem, as the first chunk will never be so big that it would touch the 2GB limit.
   if ((NewLen > AEWF_MAX_SIZE_SECTIONSECTOR) ||                    // Unlike libewf, we prefer to have the whole section "sectors" <= 2GiB (libewf allows to have the
       (pAewf->ChunkOffsets == AEWF_MAX_CHUNK_OFFSETS))             // last chunk start below the 2GiB limit and span beyond).
   {
//      printf ("\nNew table -- %llu %d %u %u", pAewf->CurrentSectionSectorContentSize, DataLen, NewLen, pAewf->ChunkOffsets);
      CHK (AewfFinishSectionSectors (pAewf))
      CHK (AewfWriteSectionTable    (pAewf))
   }

   // Write the data
   // --------------
   // Create a new section "sectors" if necessary
   if (pAewf->CurrentSectionSectorContentSize == 0)
   {
      pAewf->CurrentSectionSectorSeekPos = AewfGetCurrentSeekPos(pAewf);
      pAewf->ChunkBaseOffset             = AewfGetCurrentSeekPos(pAewf);  // We also could do this after section "sectors" has been written (thus gaining 76 bytes), but this mimics ewfacquire behaviour
      CHK (AewfWriteSection (pAewf, AEWF_SECTIONNAME_SECTORS, nullptr, 0))
   }
   OffsetArrEntry = AewfGetCurrentSeekPos(pAewf) - pAewf->ChunkBaseOffset;
   if (pPreprocess->Compressed)
      OffsetArrEntry |= AEWF_COMPRESSED_CHUNK;
//   printf ("\nWrite chunk %d -- %d bytes", pAewf->ChunkOffsets, DataLen);
   pAewf->ChunkOffsetArr[pAewf->ChunkOffsets++] = qToLittleEndian (OffsetArrEntry);
   CHK (AewfWriteFile (pAewf, pData, DataLen))
   pAewf->CurrentSectionSectorContentSize += DataLen;

//   UTIL_MEM_FREE (pPreprocess);

   return NO_ERROR;
}

APIRET AewfClose (t_pAewf *ppAewf, QList<quint64> &BadSectors, const uchar *pMD5, const uchar *pSHA1, const uchar */*pSHA256*/)
{
   t_Aewf *pAewf = *ppAewf;

   if (pAewf->pCurrentSegmentFile) // This must be checked as pCurrentSegmentFile might be zero if an error occurred (for example fwrite or fclose error during acquisition)
   {
      if (pAewf->CurrentSectionSectorContentSize)  // Is there an unfinished section "sectors"?
      {
         CHK (AewfFinishSectionSectors (pAewf))
         CHK (AewfWriteSectionTable    (pAewf))
      }

      if (pAewf->SegmentFiles == 1)                  // Add section "data" if there was only 1 segment file
         CHK (AewfWriteSectionVolume (pAewf, false))

      if (BadSectors.count())
         CHK (AewfWriteSectionError (pAewf, BadSectors))

      CHK (AewfWriteSectionDigest (pAewf, pMD5, pSHA1))
      CHK (AewfWriteSectionHash   (pAewf, pMD5))
      CHK (AewfWriteSectionDone   (pAewf))

      CHK (AewfCloseFile (pAewf))
   }
   delete pAewf;
   *ppAewf = nullptr;

   return NO_ERROR;
}

// ---------------------
//  API read functions
// ---------------------

#define CHK_LOG(pAewf, rc) \
{                          \
   int ChkRc = (rc);       \
   if (ChkRc != NO_ERROR)  \
      LOG_INFO ("AEWF verification error in file %s", QSTR_TO_PSZ(pAewf->SegmentFilenameList[pAewf->SegmentFiles-1])) \
   CHK (ChkRc)             \
}

static APIRET AewfReallocVerifyBuffer (t_Aewf *pAewf, uint Len)
{
   if (pAewf->VerifyBufferLen < Len)
   {
      pAewf->pVerifyBuffer = (uchar *) realloc (pAewf->pVerifyBuffer, Len);
      if (pAewf->pVerifyBuffer == nullptr)
         CHK (ERROR_AEWF_MEMALLOC_FAILED)
      pAewf->VerifyBufferLen = Len;
   }
   return NO_ERROR;
}

APIRET AewfOpen (t_pAewf *ppAewf, const char *pFilename)
{
   t_Aewf *pAewf;

   *ppAewf = new t_Aewf;
   pAewf = *ppAewf;
   pAewf->SegmentFilename = pFilename;

   return NO_ERROR;
}

APIRET AewfClose (t_pAewf *ppAewf)
{
   t_Aewf *pAewf;

   pAewf = *ppAewf;

   if (pAewf->pVerifyBuffer)
      free (pAewf->pVerifyBuffer);  // Must released with free, as realloc is used for allocating it

   delete (pAewf);
   *ppAewf = nullptr;

   return NO_ERROR;
}

static APIRET AewfReadFile (t_Aewf *pAewf, void *pData, uint DataLen, bool DisableMD5=false)
{
   if (fread (pData, DataLen, 1, pAewf->pCurrentSegmentFile) != 1)
      CHK_LOG (pAewf, ERROR_AEWF_CANNOT_READ_DATA)

   if (CONFIG(CalcImageFileMD5) && !DisableMD5)
      CHK (HashMD5Append (&pAewf->CurrentSegmentHashContextMD5, pData, DataLen))

   return NO_ERROR;
}

static APIRET AewfVerifyFileHeader (t_Aewf *pAewf)
{
   t_AewfFileHeader FileHeader;

   CHK (AewfReadFile (pAewf, &FileHeader, sizeof(FileHeader)))

   if (qFromLittleEndian (FileHeader.SegmentNumber) != pAewf->SegmentFiles)
      CHK_LOG (pAewf, ERROR_AEWF_WRONG_SEGMENT_NUMBER)

   return NO_ERROR;
}

static APIRET AewfReadSection (t_Aewf *pAewf, t_AewfSection *pSection, bool DisableMD5=false)
{
   unsigned int CRC;

   CHK (AewfReadFile (pAewf, pSection, sizeof(t_AewfSection), DisableMD5))
//   printf ("\nsection %s ", pSection->Name);
   CRC = AewfCRC (pSection, offsetof (t_AewfSection, CRC));
   pSection->NextSectionOffset = qFromLittleEndian (pSection->NextSectionOffset);
   pSection->SectionSize       = qFromLittleEndian (pSection->SectionSize      );
   pSection->CRC               = qFromLittleEndian (pSection->CRC              );
   if (CRC != pSection->CRC)
      CHK_LOG (pAewf, ERROR_AEWF_WRONG_SECTION_CRC)

//   printf (" --  ok");
   return NO_ERROR;
}

static APIRET AewfCheckSection (t_Aewf *pAewf, bool *pIsSectors, bool *pIsEof, bool *pIsEoi, quint64 *pNextSectionSeekPos)
{
   t_AewfSection Section;
   int           RemainingLen;
   bool          IsSectors;
   bool          IsEof;
   bool          IsEoi;
   char          EofTest;

   CHK (AewfReadSection (pAewf, &Section))
   IsSectors = (strcmp(Section.Name, AEWF_SECTIONNAME_SECTORS) == 0);
   IsEof     = (strcmp(Section.Name, AEWF_SECTIONNAME_NEXT   ) == 0);
   IsEoi     = (strcmp(Section.Name, AEWF_SECTIONNAME_DONE   ) == 0);
   if (IsEof || IsEoi)
   {                                                               // In this case, there should be no more bytes. Lets try to
      if (fread (&EofTest, 1, 1, pAewf->pCurrentSegmentFile) == 1) // read 1 more byte - this is one of the rare places where
         CHK_LOG (pAewf, ERROR_AEWF_DATA_FOLLOWING_LAST_SECTION)   // successful reading leads to an error :-)
   }
   if (!IsSectors && !IsEof && !IsEoi)  // Read remaining section data for correct segment MD5 calculation
   {
      RemainingLen = Section.SectionSize - sizeof(Section);
      if (RemainingLen)
      {
         if (RemainingLen < 0)
         CHK_LOG (pAewf, ERROR_AEWF_WRONG_SECTION_LENGTH)

//         printf ("\nCurrentSeekPos %llu - Remaining %u", AewfGetCurrentSeekPos(pAewf), RemainingLen);
         CHK (AewfReallocVerifyBuffer(pAewf, RemainingLen))
         CHK (AewfReadFile (pAewf, pAewf->pVerifyBuffer, RemainingLen))
      }
   }
   if (pIsSectors)          *pIsSectors          = IsSectors;
   if (pIsEof)              *pIsEof              = IsEof;
   if (pIsEoi)              *pIsEoi              = IsEoi;
   if (pNextSectionSeekPos) *pNextSectionSeekPos = Section.NextSectionOffset;

   return NO_ERROR;
}

static APIRET AewfReadTable (t_Aewf *pAewf)
{
   t_AewfSection              Section;
   t_AewfSectionTableContents TableContents;
   uint                       CalcCRC1;
   uint                       CalcCRC2, StoredCRC2;
   uint                       OffsetArrSize;
   quint64                    DummyOffset;

   DummyOffset = AewfGetCurrentSeekPos(pAewf);
   CHK (AewfReadSection (pAewf, &Section, true))
   if (strcmp (Section.Name, AEWF_SECTIONNAME_TABLE) != 0)
      CHK_LOG (pAewf, ERROR_AEWF_EXPECTED_SECTION_TABLE)

   CHK (AewfReadFile (pAewf, &TableContents, offsetof (t_AewfSectionTableContents, OffsetArr), true))
   CalcCRC1 = AewfCRC(&TableContents, offsetof(t_AewfSectionTableContents, CRC1));
   TableContents.Offsets    = qFromLittleEndian (TableContents.Offsets   );
   TableContents.BaseOffset = qFromLittleEndian (TableContents.BaseOffset);
   TableContents.CRC1       = qFromLittleEndian (TableContents.CRC1      );
   if (TableContents.Offsets > AEWF_MAX_CHUNK_OFFSETS)
      CHK_LOG (pAewf, ERROR_AEWF_TOO_MANY_TABLE_OFFSETS)
   if (CalcCRC1 != TableContents.CRC1)
      CHK_LOG (pAewf, ERROR_AEWF_TABLE_WRONG_CRC1)

   pAewf->CurrentChunkOffset = 0;
   pAewf->ChunkOffsets       = TableContents.Offsets;
   pAewf->ChunkBaseOffset    = TableContents.BaseOffset;
   OffsetArrSize = TableContents.Offsets * sizeof (unsigned int);
   CHK (AewfReadFile (pAewf, &pAewf->ChunkOffsetArr[0], OffsetArrSize      , true))
   CHK (AewfReadFile (pAewf, &StoredCRC2              , sizeof (StoredCRC2), true))
   StoredCRC2 = qFromLittleEndian (StoredCRC2);
   CalcCRC2   = AewfCRC (&pAewf->ChunkOffsetArr[0], OffsetArrSize);
   if (CalcCRC2 != StoredCRC2)
      CHK_LOG (pAewf, ERROR_AEWF_TABLE_WRONG_CRC2)

   pAewf->ChunkOffsetArr[TableContents.Offsets] = qToLittleEndian ((uint)(DummyOffset - TableContents.BaseOffset));

   return NO_ERROR;
}

// AewfReadNextChunk: Besides reading data chunk, the function returns image filenames and image file hashes "from time to time", i.e. when
// finishing with the data found in a segment file. If ever this is the case, all 3 params (pSegmentFilename, pMD5Digest and pMD5Valid) are correctly
// filled in. If not, SegmentFilename simply is empty.
// AewfReadNextChunk must be called again and again until pFinished is set to true, even if no data is returned. This must be done in order to have
// the function return all segment file names with correct MD5 values.

APIRET AewfReadNextChunk (t_Aewf *pAewf, uchar *pData, uint *pDataLen, QString *pSegmentFilename, t_pHashMD5Digest pMD5Digest, bool *pMD5valid, bool *pFinished)
{
   uint    Offset;
   uint    NextOffset;
   uint    ReadLen;
   uint    CalcCRC, StoredCRC;
   quint64 SavedSeekPos;
   quint64 NextSectionSeekPos;
   quint64 FullOffset;
   bool    Compressed;
   bool    IsSectors = false;
   bool    IsEof     = false;  // end of file
   bool    IsEoi     = false;  // end of image
   APIRET  rc;
   uLongf  DataLen;

   *pSegmentFilename = QString();
   *pMD5valid        = false;
   *pFinished        = false;

   // Open segment file if necessary and check file header
   // ----------------------------------------------------
   if (!pAewf->pCurrentSegmentFile)
   {
      QString Extension, Filename;

      // Open next segment file
      // ----------------------
      pAewf->SegmentFiles++;
      Extension = AewfGetExtension (pAewf->SegmentFiles);
      Filename  = pAewf->SegmentFilename + "." + Extension;
      pAewf->SegmentFilenameList += Filename;

//      printf ("\nOpening %s", QSTR_TO_PSZ(Filename));
      pAewf->pCurrentSegmentFile = fopen64 (QSTR_TO_PSZ(Filename), "r");
      if (pAewf->pCurrentSegmentFile == nullptr)
         CHK_LOG (pAewf, ERROR_AEWF_CANNOT_OPEN_FILE)

      if (CONFIG(CalcImageFileMD5))
         CHK (HashMD5Init (&pAewf->CurrentSegmentHashContextMD5))

      CHK (AewfVerifyFileHeader (pAewf))
      memset (pAewf->ChunkOffsetArr, 0, sizeof(pAewf->ChunkOffsetArr));
      pAewf->ChunkBaseOffset    = 0;
      pAewf->ChunkOffsets       = 0;
      pAewf->CurrentChunkOffset = 0;
   }

   // Check if still chunks remain; search next section "sectors" if
   // not; close segment file if no other section "sectors" is present
   // -----------------------------------------------------------------
   if (pAewf->CurrentChunkOffset == pAewf->ChunkOffsets)
   {
      do
      {
         CHK (AewfCheckSection (pAewf, &IsSectors, &IsEof, &IsEoi, &NextSectionSeekPos))    // Loop through sections until a new section "sectors" is found
      }
      while (!IsSectors && !IsEof && !IsEoi);
      if (IsEof || IsEoi)
      {
         int ret = UtilClose (pAewf->pCurrentSegmentFile);
         if (ret)
         {
            int err = errno;
            LOG_ERROR ("UtilClose returned %d (errno %d - %s)", ret, err, strerror(err));
            CHK_LOG (pAewf, ERROR_AEWF_CANNOT_CLOSE_FILE)
         }
         pAewf->pCurrentSegmentFile = nullptr;

         *pDataLen         = 0;
         *pSegmentFilename = pAewf->SegmentFilenameList[pAewf->SegmentFiles-1];
         *pFinished        = IsEoi;
         if (CONFIG(CalcImageFileMD5))
         {
            CHK (HashMD5Digest (&pAewf->CurrentSegmentHashContextMD5, pMD5Digest))
            *pMD5valid = true;
         }
         return NO_ERROR;
      }
      SavedSeekPos = AewfGetCurrentSeekPos(pAewf);
      CHK (AewfSetCurrentSeekPos (pAewf, NextSectionSeekPos))
      CHK (AewfReadTable         (pAewf))
      CHK (AewfSetCurrentSeekPos (pAewf, SavedSeekPos))
   }

   // Read next chunk
   // ---------------
//   printf ("\nReading chunk %u", pAewf->CurrentChunkOffset);
   Offset      = qFromLittleEndian (pAewf->ChunkOffsetArr[pAewf->CurrentChunkOffset++]);
   NextOffset  = qFromLittleEndian (pAewf->ChunkOffsetArr[pAewf->CurrentChunkOffset  ]);
   Compressed  = ((Offset & AEWF_COMPRESSED_CHUNK) != 0);
   Offset     &= ~AEWF_COMPRESSED_CHUNK;
   NextOffset &= ~AEWF_COMPRESSED_CHUNK;
   ReadLen     = NextOffset - Offset;
   FullOffset  = pAewf->ChunkBaseOffset + Offset;
//   printf (" -- size %u", ReadLen);

   if (AewfGetCurrentSeekPos(pAewf) != FullOffset)  // Check if table entry is correct
   {
      LOG_INFO ("AEWF section table: Offset is %llu, current seek pos is %llu", FullOffset, AewfGetCurrentSeekPos(pAewf))
      CHK_LOG (pAewf, ERROR_AEWF_TABLE_CORRUPT)
   }

   if (!Compressed)
   {
      if (ReadLen > *pDataLen)
      {
         LOG_INFO ("ChunkBaseOffset=%llu CurrentChunkOffset=%u", pAewf->ChunkBaseOffset, pAewf->CurrentChunkOffset)
         LOG_INFO ("ReadLen=%u DataLen=%u"                     , ReadLen, *pDataLen)
         LOG_INFO ("Offset=%u NextOffset=%u"                   , Offset, NextOffset)
         CHK_LOG (pAewf, ERROR_AEWF_VERIFY_BUFFER_TOO_SMALL)
      }
      CHK (AewfReadFile (pAewf, pData, ReadLen))

      *pDataLen = ReadLen - sizeof (unsigned int);  // CRC is at the end of uncompressed data
      CalcCRC   = AewfCRC (pData, *pDataLen);
      StoredCRC = qFromLittleEndian (* ((unsigned int *) (&pData[*pDataLen])));
      if (CalcCRC != StoredCRC)
         CHK_LOG (pAewf, ERROR_AEWF_DATA_CRC_ERROR)
   }
   else
   {
      CHK (AewfReallocVerifyBuffer (pAewf, ReadLen))
      CHK (AewfReadFile            (pAewf, pAewf->pVerifyBuffer, ReadLen))

      DataLen = *pDataLen;
      rc = AewfUncompress (pData, &DataLen, pAewf->pVerifyBuffer, ReadLen);

      *pDataLen = DataLen;
      if (rc != NO_ERROR)
         CHK_LOG (pAewf, rc)
   }

   return NO_ERROR;
}

// -----------------------
//      Misc. functions
// -----------------------

unsigned int AewfPreprocessExtraSpace (uint FifoBlockSize)
{
   // AEWF needs some extra FIFO block buffer space in order to do the compression or to add the CRC
   // at the end of the uncompressed data. We take the max. value of both in order to be sure that
   // enough space is available for both.

   uint ExtraZ               = UtilGetMaxZcompressedBufferSize (FifoBlockSize) - FifoBlockSize;
   uint ExtraUncompressedCRC = sizeof (unsigned int);

   return GETMAX (ExtraZ, ExtraUncompressedCRC);
}


APIRET AewfInit (void)
{
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_MEMALLOC_FAILED            ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_CREATE_FILE         ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_WRITE_FILE          ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_ANOTHER_FILE_STILL_OPEN    ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_NO_FILE_OPEN               ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_FLUSH_FILE          ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_CLOSE_FILE          ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_INVALID_COMPRESSION_LEVEL  ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_COMPRESSION_FAILED         ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_SEEK                ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_OPEN_FILE           ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_TABLE_CORRUPT              ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_VERIFY_BUFFER_TOO_SMALL    ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_CANNOT_READ_DATA           ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_DATA_CRC_ERROR             ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_UNCOMPRESS_FAILED          ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_UNCOMPRESS_ZEROBLOCK_FAILED))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_WRONG_SEGMENT_NUMBER       ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_WRONG_SECTION_CRC          ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_ENDING_SECTION_MISSING     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_EXPECTED_SECTION_TABLE     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_TABLE_WRONG_CRC1           ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_TABLE_WRONG_CRC2           ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_TOO_MANY_TABLE_OFFSETS     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_DATA_FOLLOWING_LAST_SECTION))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_WRONG_SECTION_LENGTH       ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AEWF_INVALID_EWFNAMING          ))

   // Compresison levels
   // ------------------
   switch (CONFIG(EwfCompression))
   {
      case LIBEWF_COMPRESSION_NONE : AewfZipCompressionLevel = 0; AewfEwfCompressionLevel = 0x00; break;
      case LIBEWF_COMPRESSION_EMPTY: AewfZipCompressionLevel = 0; AewfEwfCompressionLevel = 0x00; break;
      case LIBEWF_COMPRESSION_FAST : AewfZipCompressionLevel = 1; AewfEwfCompressionLevel = 0x01; break;
      case LIBEWF_COMPRESSION_BEST : AewfZipCompressionLevel = 9; AewfEwfCompressionLevel = 0x02; break;
      default: CHK_EXIT (ERROR_AEWF_INVALID_COMPRESSION_LEVEL)
   }

   // Compressed zero buffer initialisation
   // -------------------------------------
   unsigned char  *pZeroBuff;
   int              rc;

   AewfZeroBlockUnCompressedLen = CONFIG (FifoBlockSizeEWF);             // Alloc and initialise zero and compression buffers
   AewfZeroBlockCompressedLen   = UtilGetMaxZcompressedBufferSize (AewfZeroBlockUnCompressedLen);
   pZeroBuff                    = (unsigned char *) malloc (AewfZeroBlockUnCompressedLen);
   pAewfZeroBlockCompressed     = (unsigned char *) malloc (AewfZeroBlockUnCompressedLen);
   if ((pZeroBuff == nullptr) || (pAewfZeroBlockCompressed == nullptr))
      CHK (ERROR_AEWF_MEMALLOC_FAILED)
   memset (pZeroBuff, 0, AewfZeroBlockUnCompressedLen);

   rc = compress2 ((Bytef*)pAewfZeroBlockCompressed, &AewfZeroBlockCompressedLen, (Bytef*)pZeroBuff, AewfZeroBlockUnCompressedLen, 9); // Compress zero buff
   if (rc != Z_OK)
      CHK (ERROR_AEWF_COMPRESSION_FAILED)

   free (pAewfZeroBlockCompressed);                                   // Realloc compressed to the size that is really needed
   pAewfZeroBlockCompressed = (unsigned char *) malloc (AewfZeroBlockCompressedLen);
   if (pAewfZeroBlockCompressed == nullptr)
      CHK (ERROR_AEWF_MEMALLOC_FAILED)

   rc = compress2 ((Bytef*)pAewfZeroBlockCompressed, &AewfZeroBlockCompressedLen, (Bytef*)pZeroBuff, AewfZeroBlockUnCompressedLen, 9);
   if (rc != Z_OK)
      CHK (ERROR_AEWF_COMPRESSION_FAILED)

   free (pZeroBuff);

   return NO_ERROR;
}

APIRET AewfDeInit (void)
{
   free (pAewfZeroBlockCompressed);

   return NO_ERROR;
}
