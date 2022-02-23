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

#include "common.h"

#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "config.h"
#include "main.h"
#include "util.h"

#include <sys/statvfs.h>

// ----------------
//   Global vars
// ----------------

#define UTIL_ZEROBLOCK_MAXSIZE 65536

static char         *pUtilZeroBlock     = nullptr;
static unsigned int   UtilZeroBlockSize = 0;

// ----------------
//     Functions
// ----------------

void *UtilMemAlloc (size_t Size, const char */*pFile*/, int /*Line*/)
{
   return malloc(Size);
}

void UtilMemFree (void *pMem, const char */*pFile*/, int /*Line*/)
{
   free (pMem);
}

int UtilCalcDecimals (t_uint64 Value)
{
   QString Str;

   Str.setNum (Value);
   return Str.length();
}

bool UtilIsZero (unsigned char *pData, unsigned int DataLen)
{
   unsigned int ToCompare;
   unsigned int Remaining = DataLen;
   bool         IsZero    = true;

   while (Remaining)
   {
      ToCompare = GETMIN(Remaining, UtilZeroBlockSize);
      IsZero = (memcmp (pData, pUtilZeroBlock, ToCompare) == 0);
      if (!IsZero)
         break;
      Remaining -= ToCompare;
      pData     += ToCompare;
   }
   return IsZero;
}

unsigned int UtilGetMaxZcompressedBufferSize (unsigned int UncompressedBufferSize)
{
   return (unsigned int)(UncompressedBufferSize * 1.001f) + 12;
}


unsigned long long UtilGetInstalledRAM (void)
{
   unsigned long long Bytes;
   int                Pages, PageSize;

   Pages    = sysconf(_SC_PHYS_PAGES);
   PageSize = sysconf(_SC_PAGESIZE);

   if ((Pages < 0) || (PageSize < 0))
   {
      LOG_ERROR ("Amount of instlled RAM could not be determined, assume 256MB");
      Bytes = 256*1024*1024;
   }
   else
   {
      Bytes = (unsigned long long) sysconf(_SC_PHYS_PAGES) *
              (unsigned long long) sysconf(_SC_PAGESIZE  );
   }

   return Bytes;
}

unsigned int UtilGetNumberOfCPUs (void)
{
   int ret;

   ret = sysconf( _SC_NPROCESSORS_ONLN );
   if (ret<0)
   {
      LOG_ERROR ("Number of CPUs could not be determined, assume 1 CPU");
      ret = 1;
   }
   return (unsigned int) ret;
}

APIRET UtilGetFreeSpace (const QString &DirPath, quint64 *pFreeBytes)
{
   struct statvfs vfs;

   if (statvfs (DirPath.toLocal8Bit(), &vfs) == -1)
      return ERROR_UTIL_FREESPACE_UNKNOWN;

   *pFreeBytes = vfs.f_bfree * vfs.f_bsize;

   return NO_ERROR;
}

QString UtilSizeToHuman (unsigned long long Size, bool SI, int FracLen, int UnitThreshold)
{
   QString      SizeHuman;
   const char *pBaseUnit = "Byte";
   const char *pUnit;
   double       Sz;
   double       Divisor;

   if (SI)
        Divisor = 1000.0;
   else Divisor = 1024.0;

   if (UnitThreshold == -1) // auto
      UnitThreshold = (int) Divisor;

   Sz = Size;
   pUnit = pBaseUnit;
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "kB" : "KiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "MB" : "MiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "GB" : "GiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "TB" : "TiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "PB" : "PiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "EB" : "EiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "ZB" : "ZiB"; }
   if (Sz >= UnitThreshold) { Sz = Sz / Divisor; pUnit = SI ? "YB" : "YiB"; }

   if (FracLen == -1)  // auto
   {
      if (pUnit == pBaseUnit) FracLen = 0; // no frac if unit is bytes
      else if (Sz >= 100)     FracLen = 0;
      else if (Sz >= 10 )     FracLen = 1;
      else                    FracLen = 2;
   }
   SizeHuman = MainGetpNumberLocale()->toString (Sz, 'f', FracLen) + pUnit;

   return SizeHuman;
}

// UtilClose is wrapper and replacement for fclose. UtilClose was added because of rare
// fclose problems when writing to network paths.

#ifdef UTIL_CLOSE_OLD
//#define UTIL_EMULATE_CLOSE_ERROR

int UtilClose (FILE *pFile)           // This old function retries the close operation several times in case of errors. It was found
{                                     // that this is not the right to go as guymager may hang when calling fclose a second time if
   const int MaxRetries   = 10;       // the first call was unsuccessful.
   const int SleepSeconds =  5;
   int       rc, err;

   for (int i=0;;i++)
   {
      #ifdef UTIL_EMULATE_CLOSE_ERROR
         #warning "Debugging flag UTIL_EMULATE_CLOSE_ERROR is active"
         static int CloseErrorCount=0;
         if ((CloseErrorCount>=3) && (CloseErrorCount<=5))
              rc = -1;
         else rc = fclose (pFile);
         CloseErrorCount++;
      #else
         rc = fclose (pFile);
      #endif

      if (rc==0)
      {
         if (i>0)
            LOG_INFO ("fclose successful after %d tries", i+1)
         break;
      }
      err = errno;
      LOG_INFO ("fclose returned %d (errno %d - %s)", rc, err, strerror(err))

      if (i>=MaxRetries)
      {
         LOG_ERROR ("Too many fclose errors, giving up!")
         break;
      }
      sleep (SleepSeconds);
   }

   return rc;
}

#else

int UtilClose (FILE *pFile)
{
   int ret;
   int err;


   #define LOG_FUNCTION_ERROR(FnName) \
   {                                  \
      err = errno;                    \
      LOG_ERROR ("%s returned %d (errno %d - %s)", FnName, ret, err, strerror(err)) \
   }

   if CONFIG (AvoidCifsProblems)
   {
      ret = fflush (pFile);
      if (ret)
         LOG_FUNCTION_ERROR ("fflush")

      ret = fileno (pFile);
      if (ret == -1)
         LOG_FUNCTION_ERROR ("fileno")
      else
      {
         ret = fsync (ret);
         if (ret == -1)
            LOG_FUNCTION_ERROR ("fsync")
      }
   }

   ret = fclose (pFile);
   if (ret)
      LOG_FUNCTION_ERROR ("fclose")

   #undef LOG_FUNCTION_ERROR

   return ret;
}

#endif

t_UtilMem::t_UtilMem()
{
   oSize = 0;
   poMem = nullptr;
}

t_UtilMem::t_UtilMem (unsigned long long Size)
{
   oSize = Size;
   poMem = malloc (Size);
}

t_UtilMem::~t_UtilMem()
{
   if (poMem)
      free(poMem);
};

void* t_UtilMem::GetpMem (void)
{
   return poMem;
};

unsigned long long t_UtilMem::Size(void)
{
   return oSize;
}

int t_UtilMem::Set (const char *pStr)
{
   size_t StrLen = strlen (pStr) +1;
   int    ret    = 0;

   if (StrLen > (oSize))
   {
      StrLen = oSize;
      ret    = -1;
   }
   if (StrLen)
   {
      memcpy (poMem, pStr, StrLen-1);
      ((char*)poMem)[StrLen] ='\0';
   }
   return ret;
}


APIRET UtilInit (void)
{
   int MaxFifoBockSize;

   MaxFifoBockSize = GETMAX(CONFIG(FifoBlockSizeEWF), CONFIG(FifoBlockSizeAFF));

   UtilZeroBlockSize = GETMIN(UTIL_ZEROBLOCK_MAXSIZE, MaxFifoBockSize);

   pUtilZeroBlock = (char *) malloc (UtilZeroBlockSize);
   memset(pUtilZeroBlock, 0, UtilZeroBlockSize);

   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_UTIL_FREESPACE_UNKNOWN))

   return NO_ERROR;
}

APIRET UtilDeInit (void)
{
   free (pUtilZeroBlock);
   return NO_ERROR;
}
