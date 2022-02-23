// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Everything related to file names, extension names, paths,
//                  etc
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

#ifndef FILE_H
#define FILE_H

#include <QtGui> //lint !e537 Repeated include

#ifndef COMMON_H
   #include "common.h"
#endif

namespace t_File
{
   typedef enum
   {
      NotSet,
      DD,
      EWF,
      AAFF,
      AEWF = 0xAEF   // Take care not to collide with LIBEWF definitions (see also SetArrEwfFormat in config.cpp)
   } Format, t_Format;

   extern const char *pExtensionInfo;

   int    GetDdSplitNrDecimals (t_uint64 DeviceSize, t_uint64 SplitFileSize);

   APIRET GetFormatDescription (t_Format Format, bool Clone, int DdSplitDecimals, QString &Str);
   APIRET GetFormatExtension   (t_Format Format, bool Clone, int DdSplitDecimals, QString *pExtWildcards, QString *pExtHumanReadable=NULL);
   APIRET GetFormatDescription (t_pcDevice pDevice, QString &Str);
   APIRET GetFormatExtension   (t_pcDevice pDevice, QString *pExtWildCards, QString *pExtHumanReadable=NULL);

   APIRET Init                 (void);
}


// ------------------------------------
//             Error codes
// ------------------------------------

   #ifdef MODULES_H
      enum
      {
         ERROR_FILE_ONLY_CAN_BE_INSTANTIATED_ONCE = ERROR_BASE_FILE + 1,
         ERROR_FILE_INVALID_FORMAT,
         ERROR_FILE_INVALID_EWF_FORMAT
      };
   #endif

#endif


