// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Standard include file
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


#ifdef LINT_CODE_CHECK
#include "macros_for_lint.h"
#endif

#ifndef COMMON_H
#define COMMON_H

#ifndef ENABLE_LIBEWF
   #define ENABLE_LIBEWF 1
#endif

//#define USE_MD5_FROM_OPENSSL
//#define USE_SHA256_FROM_OPENSSL

// GNU C lib definitions
// ---------------------
#ifndef _LARGEFILE_SOURCE
   #define _LARGEFILE_SOURCE 1
#endif

#ifndef _FILE_OFFSET_BITS
   #define _FILE_OFFSET_BITS 64
#endif

#ifndef _GNU_SOURCE
   #define _GNU_SOURCE 1
#endif

#ifndef _THREAD_SAFE
   #define _THREAD_SAFE 1
#endif

#ifndef _STDIO_H
   #include <stdio.h>
#endif

#include <limits.h>

#include <qglobal.h>

#include "toolglobalid.h"
#include "toolerror.h"
#include "toollog.h"
#include "tooltypes.h"

#include "modules.h"
#include "error.h"

#define QSTR_TO_PSZ(QStr) (QStr).toLatin1().constData()


extern void *pOffsetOfNullPointer;
#define OFFSET_OF(Type, Element)                            \
   ((unsigned int) &(((Type*)pOffsetOfNullPointer)->Element))


class   t_Device;                    // As t_Device is the core structure of guymager and as it is needed
typedef t_Device       *t_pDevice;   // all the time, it is easiest to declare it here (eventhough I don't
typedef t_Device const *t_pcDevice;  // like that style too much, but we won't change C++ at this time).

class   t_Acquisition;
typedef t_Acquisition       *t_pAcquisition;
typedef t_Acquisition const *t_pcAcquisition;

#define EWF_MULTITHREADED_COMPRESSION_CHUNK_SIZE       32768

const unsigned long long BYTES_PER_MiB = 1ULL << 20;
const unsigned long long BYTES_PER_GiB = 1ULL << 30;
const unsigned long long BYTES_PER_TiB = 1ULL << 40;
const unsigned long long BYTES_PER_PiB = 1ULL << 50;
const unsigned long long BYTES_PER_EiB = 1ULL << 60;

const unsigned long long EWF_MIN_SEGMENT_SIZE     =   20 * BYTES_PER_MiB;
const unsigned long long EWF_MAX_SEGMENT_SIZE     = 2047 * BYTES_PER_MiB;
const unsigned long long EWF_MAX_SEGMENT_SIZE_EXT = LONG_LONG_MAX - 10 * BYTES_PER_PiB; // Let's keep some distance from the absolute maximum - just for safety.

#define GETMAX(a,b) ((a)>(b)?(a):(b))
#define GETMIN(a,b) ((a)<(b)?(a):(b))


typedef unsigned char      t_uint8;
typedef unsigned short     t_uint16;
typedef unsigned int       t_uint32;
typedef unsigned long long t_uint64;

typedef char               t_int8;
typedef short              t_int16;
typedef int                t_int32;
typedef long long          t_int64;

#endif
