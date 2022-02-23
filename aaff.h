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

#ifndef AAFF_H
#define AAFF_H

#include "hash.h"

typedef struct _t_Aaff *t_pAaff;

typedef struct
{
   bool  Zero;         // Tells AaffWrite that all data is 0
   bool  Compressed;   // Tells AaffWrite whether the data should be written compressed or uncompressed
   uint  DataLenOut;   // If Compressed is true: The size of the compressed data
} t_AaffPreprocess, *t_pAaffPreprocess;

#define AAFF_SEGNAME_COMMAND_LINE "acquisition_commandline"
#define AAFF_SEGNAME_MACADDR      "acquisition_macaddr"
#define AAFF_SEGNAME_DATE         "acquisition_date"        // Format: YYYY-MM-DD HH:MM:SS TZT
#define AAFF_SEGNAME_DEVICE       "acquisition_device"
#define AAFF_SEGNAME_MODEL        "device_model"
#define AAFF_SEGNAME_SN           "device_sn"

// ------------------------------------
//              Functions
// ------------------------------------


// Write access functions
APIRET AaffOpen         (t_pAaff *ppAaff, const char *pFilename, unsigned long long DeviceSize, unsigned int SectorSize, unsigned int PageSize);
APIRET AaffPreprocess   (t_pAaffPreprocess pPreprocess, unsigned char *pDataIn, unsigned int DataLenIn, unsigned char *pDataOut, unsigned int DataLenOut);
APIRET AaffWrite        (t_pAaff   pAaff, t_pAaffPreprocess pPreprocess, const unsigned char *pData, unsigned int DataLen);
APIRET AaffClose        (t_pAaff *ppAaff, unsigned long long BadSectors, const unsigned char *pMD5, const unsigned char *pSHA1, const unsigned char *pSHA256, uint Duration);

// Read access functions
APIRET AaffOpen         (t_pAaff *ppAaff, const char *pFilename, unsigned long long DeviceSize);
APIRET AaffReadNextPage (t_pAaff   pAaff, unsigned char *pData, unsigned int *pDataLen, QString *pImageFilename, t_pHashMD5Digest pMD5Digest, bool *pMD5valid);
APIRET AaffClose        (t_pAaff *ppAaff);


APIRET AaffWriteSegmentStr (t_pAaff pAaff, const char *pName, unsigned int Argument, const char *pStr);

APIRET AaffCopyBadSectorMarker (unsigned char *pBuffer, unsigned int Len);

// Misc. functions
unsigned int AaffPreprocessExtraSpace (unsigned int FifoBlockSize);

APIRET AaffInit   (void);
APIRET AaffDeInit (void);


// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_AAFF_CANNOT_CLOSE_FILE   = ERROR_BASE_AAFF + 1,
   ERROR_AAFF_CANNOT_CREATE_FILE,
   ERROR_AAFF_CANNOT_FLUSH_FILE,
   ERROR_AAFF_CANNOT_OPEN_FILE,
   ERROR_AAFF_CANNOT_READ_DATA,
   ERROR_AAFF_CANNOT_SEEK,
   ERROR_AAFF_CANNOT_WRITE_FILE,
   ERROR_AAFF_INVALID_FOOTER,
   ERROR_AAFF_INVALID_HEADER,
   ERROR_AAFF_INVALID_PAGE_ARGUMENT,
   ERROR_AAFF_INVALID_PAGE_NUMBER,
   ERROR_AAFF_INVALID_PAGE_ORDER,
   ERROR_AAFF_INVALID_SEGMENTLEN,
   ERROR_AAFF_INVALID_SEGMENT_NAME,
   ERROR_AAFF_INVALID_SIGNATURE,
   ERROR_AAFF_MEMALLOC_FAILED,
   ERROR_AAFF_PAGE_LARGER_THAN_BUFFER,
   ERROR_AAFF_SECTORSIZE_TOO_BIG,
   ERROR_AAFF_UNCOMPRESS_FAILED
};

#endif



