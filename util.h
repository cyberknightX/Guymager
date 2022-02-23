// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Different utility functions
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

#ifndef UTIL_H
#define UTIL_H

void *UtilMemAlloc (size_t Size, const char *pFile, int Line);
void  UtilMemFree  (void *pMem , const char *pFile, int Line);

#define UTIL_MEM_ALLOC(Size) UtilMemAlloc (Size, __FILE__, __LINE__)
#define UTIL_MEM_FREE(pMem)  UtilMemFree  (pMem, __FILE__, __LINE__)

int          UtilCalcDecimals (t_uint64 Value);
bool         UtilIsZero       (unsigned char *pData, unsigned int DataLen);
unsigned int UtilGetMaxZcompressedBufferSize (unsigned int UncompressedBufferSize);

unsigned long long UtilGetInstalledRAM (void); // Number of bytes
unsigned int       UtilGetNumberOfCPUs (void);

APIRET UtilGetFreeSpace (const QString &DirPath, quint64 *pFreeBytes);

QString UtilSizeToHuman (unsigned long long Size, bool SI=true, int FracLen=-1, int UnitThreshold=-1);

int UtilClose(FILE *pFile);

class t_UtilMem
{
   public:
      t_UtilMem ();
      t_UtilMem (unsigned long long Size);
     ~t_UtilMem ();

      unsigned long long Size     (void);
      void               *GetpMem (void);
      int                 Set     (const char *pStr);

   private:
      unsigned long long  oSize;
      void              *poMem;
};


APIRET UtilInit   (void);
APIRET UtilDeInit (void);

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_UTIL_FREESPACE_UNKNOWN = ERROR_BASE_UTIL + 1
};

#endif
