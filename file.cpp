// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Everything related to file names, extension names, paths...
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

#include <QtCore>

#include "ewf.h"

#include "util.h"
#include "device.h"
#include "config.h"

const char *t_File::pExtensionInfo = ".info";

APIRET t_File::Init (void)
{
   static bool Initialised=false;

   if (Initialised)
   {
      CHK_EXIT (ERROR_FILE_ONLY_CAN_BE_INSTANTIATED_ONCE)
   }
   else
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FILE_ONLY_CAN_BE_INSTANTIATED_ONCE))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FILE_INVALID_FORMAT))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_FILE_INVALID_EWF_FORMAT))
      Initialised = true;
   }
   return NO_ERROR;
}

APIRET t_File::GetFormatDescription (t_Format Format, bool Clone, int DdSplitDecimals, QString &Str)
{
   QString SubFormat;
   switch (Format)
   {
      case DD:
         if      (Clone)            Str = QObject::tr("Creation of a clone");
         else if (DdSplitDecimals)  Str = QObject::tr("Linux split dd raw image");
         else                       Str = QObject::tr("Linux dd raw image");
         break;
      case EWF:
         switch (CONFIG(EwfFormat))
         {
            #if (ENABLE_LIBEWF)
               case LIBEWF_FORMAT_ENCASE1: SubFormat="Encase1" ; break;
               case LIBEWF_FORMAT_ENCASE2: SubFormat="Encase2" ; break;
               case LIBEWF_FORMAT_ENCASE3: SubFormat="Encase3" ; break;
               case LIBEWF_FORMAT_ENCASE4: SubFormat="Encase4" ; break;
               case LIBEWF_FORMAT_ENCASE5: SubFormat="Encase5" ; break;
               case LIBEWF_FORMAT_ENCASE6: SubFormat="Encase6" ; break;
               case LIBEWF_FORMAT_FTK    : SubFormat="FTK"     ; break;
               case LIBEWF_FORMAT_SMART  : SubFormat="Smart"   ; break;
               case LIBEWF_FORMAT_LVF    : SubFormat="LVF"     ; break;
               case LIBEWF_FORMAT_LINEN5 : SubFormat="Linen5"  ; break;
               case LIBEWF_FORMAT_LINEN6 : SubFormat="Linen6"  ; break;
               #if (LIBEWF_VERSION >= 20130416)
                  case LIBEWF_FORMAT_ENCASE7: SubFormat="Encase7" ; break;
                  case LIBEWF_FORMAT_LINEN7 : SubFormat="Linen7"  ; break;
                  case LIBEWF_FORMAT_EWFX   : SubFormat="EWFX"    ; break;
               #endif
            #endif
            case AEWF: SubFormat="Guymager"; break;
            default  : CHK (ERROR_FILE_INVALID_EWF_FORMAT)
         }
         Str = QObject::tr("Expert Witness Format, sub-format %1") .arg(SubFormat);
         break;
      case AAFF:
         Str = QObject::tr("Advanced forensic image");
         break;
      default:  CHK (ERROR_FILE_INVALID_FORMAT)
   }
   return NO_ERROR;
}

APIRET t_File::GetFormatExtension (t_Format Format, bool Clone, int DdSplitDecimals, QString *pExtWildCards, QString *pExtHumanReadable)
{
   QString Wild, Human;

   switch (Format)
   {
      case DD:
         if (!Clone)
         {
            if (DdSplitDecimals)
            {
               Human = Wild = ".";
               while (DdSplitDecimals--)
               {
                  Human += "x";
                  Wild  += "?";
               }
            }
            else
            {
               Human = Wild = ".dd";
            }
         }
         break;
      case EWF:
         switch (CONFIG(EwfFormat))
         {
            #if (ENABLE_LIBEWF)
               case LIBEWF_FORMAT_SMART  : Wild=".s??"; Human=".sxx"; break;
               case LIBEWF_FORMAT_ENCASE1:
               case LIBEWF_FORMAT_ENCASE2:
               case LIBEWF_FORMAT_ENCASE3:
               case LIBEWF_FORMAT_ENCASE4:
               case LIBEWF_FORMAT_ENCASE5:
               case LIBEWF_FORMAT_ENCASE6:
               case LIBEWF_FORMAT_LINEN5 :
               case LIBEWF_FORMAT_LINEN6 :
               #if (LIBEWF_VERSION >= 20130416)
                  case LIBEWF_FORMAT_ENCASE7:
                  case LIBEWF_FORMAT_LINEN7 :
                  case LIBEWF_FORMAT_EWFX   :
               #endif
               case LIBEWF_FORMAT_FTK :
            #endif
            case AEWF                 : Wild=".???"; Human=".Exx"; break;  // Attention: Wildcard is not E??, as segmentated image might continue beyond .EZZ
            default                   : CHK (ERROR_FILE_INVALID_EWF_FORMAT)
         }
         break;
      case AAFF:
         Wild = ".aff";
         Human = Wild;
         break;
      case NotSet:
         Wild = "--";
         Human = Wild;
         break;
      default:  CHK (ERROR_FILE_INVALID_FORMAT)
   }

   if (pExtWildCards    ) *pExtWildCards     = Wild;
   if (pExtHumanReadable) *pExtHumanReadable = Human;

   return NO_ERROR;
}

int t_File::GetDdSplitNrDecimals (t_uint64 DeviceSize, t_uint64 SplitFileSize)
{
   int Decimals=0;

   if (SplitFileSize)
   {
      Decimals = UtilCalcDecimals (1 + DeviceSize / SplitFileSize);
      Decimals = GETMAX (Decimals, 3);
   }
   return Decimals;
}

APIRET t_File::GetFormatDescription (t_pcDevice pDevice, QString &Str)
{
   CHK (GetFormatDescription (pDevice->Acquisition1.Format,
                              pDevice->Acquisition1.Clone,
                              GetDdSplitNrDecimals (pDevice->Size, pDevice->Acquisition1.SplitFileSize),
                              Str))
   return NO_ERROR;
}

APIRET t_File::GetFormatExtension (t_pcDevice pDevice, QString *pExtWildCards, QString *pExtHumanReadable)
{
   CHK (GetFormatExtension (pDevice->Acquisition1.Format,
                            pDevice->Acquisition1.Clone,
                            GetDdSplitNrDecimals (pDevice->Size, pDevice->Acquisition1.SplitFileSize),
                            pExtWildCards, pExtHumanReadable))
   return NO_ERROR;
}
