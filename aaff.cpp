// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Multithreaded AFF (AAFF = Advanced AFF)
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
#include <time.h>    //lint !e537 !e451 repeated include file
#include <stdlib.h>
#include <zlib.h>

#include <QString>

#include "util.h"
#include "config.h"
#include "aaff.h"

// -----------------
//  AFF definitions
// -----------------

#define AFF_GID_LENGTH   16
#define AFF_SEGARG_U64   2  // Used as argument for segments that contain a 64 bit unsigned in the data field

#define AFF_HEADER                    "AFF10\r\n"
#define AFF_HEADER_LEN                7
#define AFF_SEGMENT_HEADER_MAGIC      "AFF"
#define AFF_SEGMENT_FOOTER_MAGIC      "ATT"
#define AFF_BADSECTOR_HEADER          "BAD SECTOR"
#define AFF_FILE_TYPE                 "AFF"

#define AFF_SEGNAME_BADFLAG           "badflag"
#define AFF_SEGNAME_AFFLIB_VERSION    "afflib_version"
#define AFF_SEGNAME_FILETYPE          "aff_file_type"
#define AFF_SEGNAME_GID               "image_gid"
#define AFF_SEGNAME_SECTORS           "devicesectors"
#define AFF_SEGNAME_SECTORSIZE        "sectorsize"
#define AFF_SEGNAME_IMAGESIZE         "imagesize"
#define AFF_SEGNAME_PAGESIZE          "pagesize"
#define AFF_SEGNAME_BADSECTORS        "badsectors"
#define AFF_SEGNAME_MD5               "md5"
#define AFF_SEGNAME_SHA1              "sha1"
#define AFF_SEGNAME_SHA256            "sha256"
#define AFF_SEGNAME_DURATION          "acquisition_seconds"
#define AFF_SEGNAME_PAGE              "page"

#define AFF_PAGEFLAGS_UNCOMPRESSED    0x0000
#define AFF_PAGEFLAGS_COMPRESSED_ZLIB 0x0001
#define AFF_PAGEFLAGS_COMPRESSED_ZERO 0x0033    // Compressed, Zero and MaxCompression

typedef struct
{
   char         Magic[4];
   unsigned int NameLen;
   unsigned int DataLen;
   unsigned int Argument;          // Named "flags" in original aff source, named "arg" in afinfo output.
   char         Name[];
} __attribute__ ((packed)) t_AffSegmentHeader;

// Between header and footer lie the segment name and the data

typedef struct
{
   char         Magic[4];
   unsigned int SegmentLen;
} __attribute__ ((packed)) t_AffSegmentFooter;


// ------------------
//  Aaff definitions
// ------------------

#pragma GCC diagnostic push
#ifdef Q_CREATOR_RUN
#pragma GCC diagnostic ignored "-Wgnu-variable-sized-type-not-at-end"
#endif
typedef struct _t_Aaff
{
   FILE               *pFile;
   bool                 OpenedForWrite;
   unsigned long long   PagesWritten;
   unsigned long long   PagesRead;
   t_HashContextMD5     HashContextMD5; // For image file MD5 calculation during verification
   QString            *pFilename;

   unsigned char      *pUncompressBuffer;
   unsigned int         UncompressBufferLen;
   unsigned long long   BytesVerified;
   unsigned long long   DeviceSize;
   t_AffSegmentFooter   SegmentFooter;  // This header and footer are allocated and initialised only once and re-used afterwards;
   t_AffSegmentHeader   SegmentHeader;  // SegmentHeader.Name isn't used, so ignore compiler warning about flexible array members
} t_Aaff;
#pragma GCC diagnostic pop

#define AAFF_MD5_LEN                16
#define AAFF_SHA1_LEN               20
#define AAFF_SHA256_LEN             32
#define AAFF_BADSECTORMARKER_MAXLEN 65536
static unsigned char AaffBadSectorMarker[AAFF_BADSECTORMARKER_MAXLEN];

// ----------------
//  Error handling
// ----------------

#define CHK_FWRITE(Fn)                    \
   if ((Fn) != 1)                         \
      CHK (ERROR_AAFF_CANNOT_WRITE_FILE)

// -------------------
//  Segment functions
// -------------------

static APIRET AaffWriteSegment (t_pAaff pAaff, const char *pName, unsigned int Argument, const unsigned char *pData, unsigned int DataLen)
{
   unsigned int NameLen0 = (unsigned int) strlen(pName);

   pAaff->SegmentHeader.NameLen    = htonl(NameLen0);
   pAaff->SegmentHeader.DataLen    = htonl(DataLen);
   pAaff->SegmentHeader.Argument   = htonl(Argument);
   pAaff->SegmentFooter.SegmentLen = htonl(sizeof(t_AffSegmentHeader) + sizeof(t_AffSegmentFooter) + NameLen0 + DataLen);

   CHK_FWRITE(fwrite (&pAaff->SegmentHeader, sizeof(t_AffSegmentHeader), 1, pAaff->pFile))
   CHK_FWRITE(fwrite (pName, NameLen0, 1, pAaff->pFile))
   if (pData && DataLen)
      CHK_FWRITE(fwrite (pData, DataLen, 1, pAaff->pFile))
   CHK_FWRITE(fwrite (&pAaff->SegmentFooter, sizeof(t_AffSegmentFooter), 1, pAaff->pFile))

   return NO_ERROR;
}

APIRET AaffWriteSegmentStr (t_pAaff pAaff, const char *pName, unsigned int Argument, const char *pStr)
{
   CHK (AaffWriteSegment (pAaff, pName, Argument, (const unsigned char *) pStr, (unsigned int) strlen (pStr)))
   return NO_ERROR;
}

static APIRET AaffWriteSegmentArg (t_pAaff pAaff, const char *pName, unsigned int Argument)
{
   CHK (AaffWriteSegment (pAaff, pName, Argument, nullptr, 0))
   return NO_ERROR;
}

static APIRET AaffWriteSegmentU64 (t_pAaff pAaff, const char *pName, unsigned long long Value)
{
   unsigned int Data[2];

   Data[0] = htonl ((unsigned int)(Value &  0xFFFFFFFF));
   Data[1] = htonl ((unsigned int)(Value >> 32));

   CHK (AaffWriteSegment (pAaff, pName, AFF_SEGARG_U64, (unsigned char *)&Data[0], sizeof(Data)))

   return NO_ERROR;
}

static APIRET AaffWriteSegmentGID (t_pAaff pAaff)
{
   unsigned char GID[AFF_GID_LENGTH];
   int           i;

   for (i=0; i<AFF_GID_LENGTH; i++)
      GID[i] = (unsigned char) random();

   CHK (AaffWriteSegment (pAaff, AFF_SEGNAME_GID, 0, GID, AFF_GID_LENGTH))

   return NO_ERROR;
}

// ---------------
//  API functions
// ---------------

APIRET AaffCopyBadSectorMarker (unsigned char *pBuffer, unsigned int Len)
{
   if (Len > AAFF_BADSECTORMARKER_MAXLEN)
      CHK (ERROR_AAFF_SECTORSIZE_TOO_BIG)

   memcpy (pBuffer, &AaffBadSectorMarker[0], Len);

   return NO_ERROR;
}

// ---------------------
//  API write functions
// ---------------------

static APIRET AaffCreateHandle (t_pAaff *ppAaff)
{
   t_pAaff pAaff;

   pAaff = (t_pAaff) UTIL_MEM_ALLOC(sizeof(t_Aaff));
   if (pAaff == nullptr)
      CHK (ERROR_AAFF_MEMALLOC_FAILED)

   memset (pAaff, 0, sizeof(t_Aaff));

   pAaff->pFilename = new QString;

   *ppAaff = pAaff;

   return NO_ERROR;
}

static APIRET AaffDestroyHandle (t_pAaff *ppAaff)
{
   t_pAaff pAaff = *ppAaff;

   if (pAaff->pUncompressBuffer)
      free (pAaff->pUncompressBuffer);  // Must released with free, as realloc is used for allocating it

   delete pAaff->pFilename;

   memset (pAaff, 0, sizeof(t_Aaff));
   UTIL_MEM_FREE (pAaff);
   *ppAaff = nullptr;

   return NO_ERROR;
}

//lint -save -esym(613,pAaff)   Possible use of null pointer pAaff
APIRET AaffOpen (t_pAaff *ppAaff, const char *pFilename, unsigned long long DeviceSize, unsigned int SectorSize, unsigned int PageSize)
{
   t_pAaff pAaff;
   char     Buff[512];

   *ppAaff = nullptr;

   // Open file and intialise
   // -----------------------
   if (SectorSize > AAFF_BADSECTORMARKER_MAXLEN)
      CHK (ERROR_AAFF_SECTORSIZE_TOO_BIG)

   CHK (AaffCreateHandle (&pAaff))

   *(pAaff->pFilename) = pFilename;
   pAaff->OpenedForWrite = true;
   pAaff->pFile = fopen64 (pFilename, "w");
   if (pAaff->pFile == nullptr)
   {
      CHK (AaffDestroyHandle (ppAaff))
      CHK (ERROR_AAFF_CANNOT_CREATE_FILE)
   }
   *ppAaff = pAaff;

   CHK_FWRITE(fwrite (AFF_HEADER, sizeof(AFF_HEADER), 1, pAaff->pFile))

   pAaff->PagesWritten = 0;
   memset (&pAaff->SegmentHeader, 0, sizeof (t_AffSegmentHeader));
   memset (&pAaff->SegmentFooter, 0, sizeof (t_AffSegmentFooter));
   strcpy (&pAaff->SegmentHeader.Magic[0], AFF_SEGMENT_HEADER_MAGIC);
   strcpy (&pAaff->SegmentFooter.Magic[0], AFF_SEGMENT_FOOTER_MAGIC);

   // Write standard segments
   // -----------------------
   snprintf (&Buff[0], sizeof(Buff), "aaff module of Guymager %s", pCompileInfoVersion);
   CHK (AaffWriteSegmentGID (pAaff))
   if CONFIG (AffMarkBadSectors)
      CHK (AaffWriteSegment (pAaff, AFF_SEGNAME_BADFLAG       , 0, AaffBadSectorMarker, SectorSize))
   CHK (AaffWriteSegmentStr (pAaff, AFF_SEGNAME_AFFLIB_VERSION, 0, &Buff[0]))
   CHK (AaffWriteSegmentStr (pAaff, AFF_SEGNAME_FILETYPE      , 0, AFF_FILE_TYPE))
   CHK (AaffWriteSegmentArg (pAaff, AFF_SEGNAME_PAGESIZE      , PageSize))
   CHK (AaffWriteSegmentArg (pAaff, AFF_SEGNAME_SECTORSIZE    , SectorSize))
   CHK (AaffWriteSegmentU64 (pAaff, AFF_SEGNAME_SECTORS       , DeviceSize / SectorSize))
   CHK (AaffWriteSegmentU64 (pAaff, AFF_SEGNAME_IMAGESIZE     , DeviceSize))

   return NO_ERROR;
}
//lint -restore

APIRET AaffClose (t_pAaff *ppAaff, unsigned long long BadSectors, const unsigned char *pMD5, const unsigned char *pSHA1, const unsigned char *pSHA256, uint Duration)
{
   t_pAaff pAaff = *ppAaff;

   CHK (AaffWriteSegmentU64 (pAaff, AFF_SEGNAME_BADSECTORS, BadSectors))
   CHK (AaffWriteSegment    (pAaff, AFF_SEGNAME_MD5       , 0, pMD5   , AAFF_MD5_LEN   ))
   CHK (AaffWriteSegment    (pAaff, AFF_SEGNAME_SHA1      , 0, pSHA1  , AAFF_SHA1_LEN  ))
   CHK (AaffWriteSegment    (pAaff, AFF_SEGNAME_SHA256    , 0, pSHA256, AAFF_SHA256_LEN))
   CHK (AaffWriteSegmentArg (pAaff, AFF_SEGNAME_DURATION  , Duration))

   if (fflush (pAaff->pFile))
   {
      (void) fclose (pAaff->pFile);
      CHK (ERROR_AAFF_CANNOT_FLUSH_FILE)
   }

   if (fclose (pAaff->pFile))
      CHK (ERROR_AAFF_CANNOT_CLOSE_FILE)

   CHK (AaffDestroyHandle (ppAaff))

   return NO_ERROR;
}

//lint -save -esym(613,pPreProcess)   Possible use of null pointer
APIRET AaffPreprocess (t_pAaffPreprocess pPreprocess, unsigned char *pDataIn, unsigned int DataLenIn, unsigned char *pDataOut, uint DataLenOut)
{
//   t_pAaffPreprocess pPreProcess;
   int                rc;
   uLongf             LenOut;

//   *ppPreprocess = nullptr;
//   pPreProcess = (t_pAaffPreprocess) UTIL_MEM_ALLOC(sizeof(t_AaffPreprocess));
//   if (pPreProcess == nullptr)
//      CHK (ERROR_AAFF_MEMALLOC_FAILED)
//   *ppPreprocess = pPreProcess;
   pPreprocess->Zero       = false;
   pPreprocess->Compressed = false;

   // Check if zero
   // -------------
   pPreprocess->Zero = UtilIsZero (pDataIn, DataLenIn);
   if (pPreprocess->Zero)
      return NO_ERROR;

   // Try compression
   // ---------------
   LenOut = DataLenOut;
   rc = compress2 ((Bytef *)pDataOut, &LenOut, (Bytef *)pDataIn, DataLenIn, CONFIG (AffCompression));
   pPreprocess->DataLenOut = (uint) LenOut;
   if (rc != Z_OK)
   {
      if (rc != Z_BUF_ERROR)    // Do not log this one (the destination buffer was too small for the compressed result)
         LOG_ERROR ("compress2 returned %d", rc)
      return NO_ERROR;
   }

   pPreprocess->Compressed = (LenOut < DataLenIn);

   return NO_ERROR;
}
//lint -restore

APIRET AaffWrite (t_pAaff pAaff, t_pAaffPreprocess pPreprocess, const unsigned char *pData, unsigned int DataLen)
{
   char SegmentName[64];

   snprintf (&SegmentName[0], sizeof(SegmentName), "%s%llu", AFF_SEGNAME_PAGE, pAaff->PagesWritten++);

   if (pPreprocess->Zero)
   {
      unsigned int Len = htonl(DataLen);
      CHK (AaffWriteSegment (pAaff, &SegmentName[0], AFF_PAGEFLAGS_COMPRESSED_ZERO, (unsigned char *) &Len, sizeof (Len)))
   }
   else if (pPreprocess->Compressed)
        CHK (AaffWriteSegment (pAaff, &SegmentName[0], AFF_PAGEFLAGS_COMPRESSED_ZLIB, pData, pPreprocess->DataLenOut))
   else CHK (AaffWriteSegment (pAaff, &SegmentName[0], AFF_PAGEFLAGS_UNCOMPRESSED   , pData, DataLen))

//   UTIL_MEM_FREE (pPreprocess);

   return NO_ERROR;
}

// ---------------------
//  API read functions
// ---------------------

static APIRET AaffReallocUncompressBuffer (t_pAaff pAaff, unsigned int NewLen)
{
   if (NewLen > pAaff->UncompressBufferLen)
   {
      pAaff->pUncompressBuffer = (unsigned char *) realloc (pAaff->pUncompressBuffer, NewLen);
      if (pAaff->pUncompressBuffer == nullptr)
         return ERROR_AAFF_MEMALLOC_FAILED;
      pAaff->UncompressBufferLen = NewLen;
   }
   return NO_ERROR;
}

static unsigned long long AaffGetCurrentSeekPos (t_Aaff *pAaff)
{
   return (quint64) ftello64 (pAaff->pFile);
}

APIRET AaffGetImageFileSize  (t_Aaff *pAaff, unsigned long long *pSize)
{
   unsigned long long CurrentSeekPos;
   int                rc;

   CurrentSeekPos = AaffGetCurrentSeekPos (pAaff);
   rc = fseeko64 (pAaff->pFile, 0, SEEK_END);
   if (rc)
      return ERROR_AAFF_CANNOT_SEEK;

   *pSize = AaffGetCurrentSeekPos (pAaff);

   rc = fseeko64 (pAaff->pFile, (__off64_t) CurrentSeekPos, SEEK_SET);
   if (rc)
      return ERROR_AAFF_CANNOT_SEEK;

   return NO_ERROR;
}

static APIRET AaffReadFile (t_Aaff *pAaff, void *pData, unsigned int DataLen)
{
   if (fread (pData, DataLen, 1, pAaff->pFile) != 1)
      CHK (ERROR_AAFF_CANNOT_READ_DATA)

   if (CONFIG(CalcImageFileMD5))
      CHK (HashMD5Append (&pAaff->HashContextMD5, pData, DataLen))

   return NO_ERROR;
}

APIRET AaffOpen (t_pAaff *ppAaff, const char *pFilename, unsigned long long DeviceSize)
{
   t_pAaff pAaff;
   char     Signature[AFF_HEADER_LEN+1];

   *ppAaff = nullptr;
   CHK (AaffCreateHandle (&pAaff))

   pAaff->OpenedForWrite = false;
   pAaff->pUncompressBuffer   = nullptr;
   pAaff->UncompressBufferLen = 0;
   pAaff->PagesRead           = 0;
   pAaff->BytesVerified       = 0;
   pAaff->DeviceSize          = DeviceSize;
   *(pAaff->pFilename)        = pFilename;

   if (CONFIG(CalcImageFileMD5))
      CHK (HashMD5Init (&pAaff->HashContextMD5))

   pAaff->pFile = fopen64 (pFilename, "r");
   if (pAaff->pFile == nullptr)
   {
      UTIL_MEM_FREE (pAaff);
      CHK (ERROR_AAFF_CANNOT_OPEN_FILE)
   }

   // Check signature
   // ---------------
   CHK (AaffReadFile (pAaff, &Signature, sizeof(Signature)))
   if (memcmp (Signature, AFF_HEADER, sizeof(Signature)) != 0)
      return ERROR_AAFF_INVALID_SIGNATURE;

   *ppAaff = pAaff;

   return NO_ERROR;
}

APIRET AaffClose (t_pAaff *ppAaff)
{
   if (fclose ((*ppAaff)->pFile))
      CHK (ERROR_AAFF_CANNOT_CLOSE_FILE)

   CHK (AaffDestroyHandle (ppAaff))

   return NO_ERROR;
}


APIRET AaffReadNextPage (t_pAaff pAaff, unsigned char *pData, unsigned int *pDataLen, QString *pImageFilename, t_pHashMD5Digest pMD5Digest, bool *pMD5Valid)
{
   t_AffSegmentHeader Header;
   t_AffSegmentFooter Footer;
   char               SegmentName[100];
   char             *pSegmentNamePageNumber;
   char             *pTail;
   bool               Found=false;
   uLongf             DataLen = *pDataLen;
   int                rc;
   unsigned long long BytesToNextHeader;

   *pImageFilename = QString();
   *pMD5Valid      = false;

   // Search for the next segment whose name starts with "page"
   // ---------------------------------------------------------
   do
   {
      CHK (AaffReadFile (pAaff, &Header, offsetof(t_AffSegmentHeader, Name)))
      if (strcmp (&Header.Magic[0], AFF_SEGMENT_HEADER_MAGIC) != 0)
         return ERROR_AAFF_INVALID_HEADER;
      Header.NameLen  = ntohl (Header.NameLen );
      Header.DataLen  = ntohl (Header.DataLen );
      Header.Argument = ntohl (Header.Argument);

      if (Header.NameLen >= sizeof(SegmentName))
         return ERROR_AAFF_INVALID_SEGMENT_NAME;

      CHK (AaffReadFile (pAaff, &SegmentName[0], Header.NameLen))
      SegmentName[Header.NameLen] = '\0';

      Found            = (strncmp (&SegmentName[0], AFF_SEGNAME_PAGE, strlen(AFF_SEGNAME_PAGE)) == 0); // The segment name must start with "page"
      if (Found) Found = (strlen  (&SegmentName[0]) > strlen(AFF_SEGNAME_PAGE));                       // The string "page" must be followed by at least 1 digit
      if (Found) Found = isdigit  ( SegmentName[strlen(AFF_SEGNAME_PAGE)]);                            // Check if the following char is a digit (Some checking done after strtol, see below)

      if (!Found)
      {
         BytesToNextHeader = Header.DataLen + sizeof(t_AffSegmentFooter);
         if (CONFIG (CalcImageFileMD5))          // Read the data between current pos and next header in order have image file MD5 calculated correctly
         {
            CHK (AaffReallocUncompressBuffer (pAaff, BytesToNextHeader))
            CHK (AaffReadFile                (pAaff, pAaff->pUncompressBuffer, BytesToNextHeader))
         }
         else                                   // Simply seek to next header, which is faster
         {
            rc = fseeko64 (pAaff->pFile, BytesToNextHeader, SEEK_CUR);
            if (rc)
               return ERROR_AAFF_CANNOT_SEEK;
         }
      }
   } while (!Found);

   // Check page number
   // -----------------
   pSegmentNamePageNumber  = &SegmentName[strlen(AFF_SEGNAME_PAGE)];
   unsigned int PageNumber = strtol (pSegmentNamePageNumber, &pTail, 10);
   if (*pTail != '\0')                 return ERROR_AAFF_INVALID_PAGE_NUMBER;  // There should be no extra chars after the number
   if (PageNumber != pAaff->PagesRead) return ERROR_AAFF_INVALID_PAGE_ORDER;
   pAaff->PagesRead++;

   // Get data
   // --------
   if (Header.DataLen > *pDataLen)
      return ERROR_AAFF_PAGE_LARGER_THAN_BUFFER;

   switch (Header.Argument)
   {
      case AFF_PAGEFLAGS_UNCOMPRESSED:
         CHK (AaffReadFile (pAaff, pData, Header.DataLen))
         *pDataLen = Header.DataLen;
         break;
      case AFF_PAGEFLAGS_COMPRESSED_ZERO:
         unsigned int Len;
         CHK (AaffReadFile (pAaff, &Len, sizeof(Len)))
         Len = ntohl (Len);
         memset (pData, 0, Len);
         *pDataLen = Len;
         break;
      case AFF_PAGEFLAGS_COMPRESSED_ZLIB:
         CHK (AaffReallocUncompressBuffer (pAaff, Header.DataLen))
         CHK (AaffReadFile                (pAaff, pAaff->pUncompressBuffer, Header.DataLen))
         rc = uncompress (pData, &DataLen, pAaff->pUncompressBuffer, Header.DataLen);
         *pDataLen = DataLen;
         if (rc != Z_OK)
         {
            LOG_ERROR ("Zlib uncompress returned %d", rc)
               return ERROR_AAFF_UNCOMPRESS_FAILED;
         }
         break;
      default:
         LOG_INFO ("Invalid page argument: %d", Header.Argument)
         CHK (ERROR_AAFF_INVALID_PAGE_ARGUMENT)
   }

   // Check footer
   // ------------
   CHK (AaffReadFile (pAaff, &Footer, sizeof(Footer)))
   if (strcmp (&Footer.Magic[0], AFF_SEGMENT_FOOTER_MAGIC) != 0)
      return ERROR_AAFF_INVALID_FOOTER;

   unsigned int SegmentLen = sizeof (t_AffSegmentHeader) + sizeof(t_AffSegmentFooter) + Header.NameLen + Header.DataLen;
   if (ntohl(Footer.SegmentLen) != SegmentLen)
      return ERROR_AAFF_INVALID_SEGMENTLEN;

   // Check if all data has been read
   // -------------------------------
   pAaff->BytesVerified += *pDataLen;
   if (pAaff->BytesVerified == pAaff->DeviceSize)
   {
      if (CONFIG(CalcImageFileMD5))
      {
         unsigned long long ImageFileSize;
         unsigned long long BytesRemaining;

         CHK (AaffGetImageFileSize(pAaff, &ImageFileSize))
         BytesRemaining = ImageFileSize - AaffGetCurrentSeekPos(pAaff);
         CHK (AaffReadFile (pAaff, pAaff->pUncompressBuffer, BytesRemaining))
         CHK (HashMD5Digest (&pAaff->HashContextMD5, pMD5Digest))
         *pMD5Valid = true;
      }
      *pImageFilename = *(pAaff->pFilename);
   }

   return NO_ERROR;
}


// -----------------------
//      Misc. functions
// -----------------------

unsigned int AaffPreprocessExtraSpace (unsigned int FifoBlockSize)
{
   return UtilGetMaxZcompressedBufferSize (FifoBlockSize) - FifoBlockSize;
}

APIRET AaffInit (void)
{
   int i;

   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_CLOSE_FILE      ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_CREATE_FILE     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_FLUSH_FILE      ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_OPEN_FILE       ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_READ_DATA       ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_SEEK            ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_CANNOT_WRITE_FILE      ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_FOOTER         ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_HEADER         ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_PAGE_ARGUMENT  ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_PAGE_NUMBER    ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_PAGE_ORDER     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_SEGMENTLEN     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_SEGMENT_NAME   ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_INVALID_SIGNATURE      ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_MEMALLOC_FAILED        ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_PAGE_LARGER_THAN_BUFFER))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_SECTORSIZE_TOO_BIG     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_AAFF_UNCOMPRESS_FAILED      ))

   srandom ((unsigned int) time (nullptr));

   for (i=0; i<AAFF_BADSECTORMARKER_MAXLEN; i++)
      AaffBadSectorMarker[i] = (unsigned char)random();
   strcpy ((char *)AaffBadSectorMarker, AFF_BADSECTOR_HEADER);

   return NO_ERROR;
}

APIRET AaffDeInit (void)
{
   return NO_ERROR;
}

