// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Header file for including and checking libewf
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

#ifndef EWF_H
#define EWF_H

#if (ENABLE_LIBEWF)
   #include <libewf.h>

   #if ((LIBEWF_VERSION != 20080501) && (LIBEWF_VERSION != 20100226) && (LIBEWF_VERSION != 20111015) && (LIBEWF_VERSION < 20130416))
      #error "Please check EWF documentation for newer Encase formats and adjust following code"
   #endif

   #ifndef LIBEWF_HANDLE
      #define LIBEWF_HANDLE libewf_handle_t
   #endif
#else
   #define LIBEWF_COMPRESSION_NONE 0
   #define LIBEWF_COMPRESSION_FAST 1
   #define LIBEWF_COMPRESSION_BEST 2
#endif

#endif

