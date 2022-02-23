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

#ifndef AEWF_H
#define AEWF_H

#ifndef HASH_H
#include "hash.h"
#endif

class t_Aewf;
typedef t_Aewf *t_pAewf;

typedef struct
{
   bool Compressed;   // Tells AewfWrite whether the data should be written compressed or uncompressed
   int  DataLenOut;   // If Compressed - is true:  The size of the compressed data
                      //               - is false: The size of the original data + size of CRC
} t_AewfPreprocess, *t_pAewfPreprocess;

typedef enum
{
   AEWF_MEDIATYPE_REMOVABLE = 0x00,
   AEWF_MEDIATYPE_FIXED     = 0x01,
   AEWF_MEDIATYPE_OPTICAL   = 0x03
} t_AewfMediaType;

typedef enum
{
   AEWF_MEDIAFLAGS_IMAGE          = 0x01,
   AEWF_MEDIAFLAGS_PHYSICAL       = 0x02,
   AEWF_MEDIAFLAGS_FASTBLOCK_USED = 0x04,
   AEWF_MEDIAFLAGS_TABLEAU_USED   = 0x08
} t_AewfMediaFlags;

// ------------------------------------
//              Functions
// ------------------------------------

// Write access functions
// ----------------------
APIRET AewfOpen (t_pAewf *ppAewf, const char *pFilename,
                                  quint64 DeviceSize, quint64 SegmentSize,
                                  uint    ChunkSize , uint    SectorSize,
                                  t_AewfMediaType    MediaType,
                                  t_AewfMediaFlags   MediaFlags,
                                  QString Description, QString CaseNumber,  QString EvidenceNumber, QString Examiner,
                                  QString Notes,       QString DeviceModel, QString SerialNumber,   QString ImagerVersion,
                                  QString OSVersion);
APIRET AewfPreprocess    (                 t_pAewfPreprocess pPreprocess, uchar *pDataIn, uint DataLenIn, uchar *pDataOut, uint DataLenOut);
APIRET AewfWrite         (t_pAewf   pAewf, t_pAewfPreprocess pPreprocess, const uchar *pData, uint DataLen);
APIRET AewfClose         (t_pAewf *ppAewf, QList<quint64> &BadSectors, const uchar *pMD5, const uchar *pSHA1, const uchar *pSHA256);

// Read access functions
// ---------------------
APIRET AewfOpen          (t_pAewf *ppAewf, const char *pFilename);
APIRET AewfReadNextChunk (t_pAewf   pAewf, uchar *pData, uint *pDataLen, QString *pSegmentFilename, t_pHashMD5Digest pMD5, bool *pMD5valid, bool *pFinished);
APIRET AewfClose         (t_pAewf *ppAewf);

// Misc. functions
// ---------------
uint AewfPreprocessExtraSpace (uint FifoBlockSize);

APIRET AewfInit   (void);
APIRET AewfDeInit (void);


// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_AEWF_MEMALLOC_FAILED = ERROR_BASE_AEWF + 1,
   ERROR_AEWF_CANNOT_CREATE_FILE,
   ERROR_AEWF_CANNOT_WRITE_FILE,
   ERROR_AEWF_ANOTHER_FILE_STILL_OPEN,
   ERROR_AEWF_NO_FILE_OPEN,
   ERROR_AEWF_CANNOT_FLUSH_FILE,
   ERROR_AEWF_CANNOT_CLOSE_FILE,
   ERROR_AEWF_INVALID_COMPRESSION_LEVEL,
   ERROR_AEWF_COMPRESSION_FAILED,
   ERROR_AEWF_CANNOT_SEEK,
   ERROR_AEWF_CANNOT_OPEN_FILE,
   ERROR_AEWF_TABLE_CORRUPT,
   ERROR_AEWF_VERIFY_BUFFER_TOO_SMALL,
   ERROR_AEWF_CANNOT_READ_DATA,
   ERROR_AEWF_DATA_CRC_ERROR,
   ERROR_AEWF_UNCOMPRESS_FAILED,
   ERROR_AEWF_UNCOMPRESS_ZEROBLOCK_FAILED,
   ERROR_AEWF_WRONG_SEGMENT_NUMBER,
   ERROR_AEWF_WRONG_SECTION_CRC,
   ERROR_AEWF_ENDING_SECTION_MISSING,
   ERROR_AEWF_EXPECTED_SECTION_TABLE,
   ERROR_AEWF_TABLE_WRONG_CRC1,
   ERROR_AEWF_TABLE_WRONG_CRC2,
   ERROR_AEWF_TOO_MANY_TABLE_OFFSETS,
   ERROR_AEWF_DATA_FOLLOWING_LAST_SECTION,
   ERROR_AEWF_WRONG_SECTION_LENGTH,
   ERROR_AEWF_INVALID_EWFNAMING
};

#endif

