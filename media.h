// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Module for special media function, like HPA/DCO and others
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

#ifndef MEDIA_H
#define MEDIA_H

#include <QString>

class t_MediaInfo
{
   public:
       t_MediaInfo ();
      ~t_MediaInfo ();

       APIRET QueryDevice (const char *pLinuxDevice);

   private:
      void InitUnknown (void);

   public:
      typedef enum
      {
         Unknown = 0,
         No,
         Yes
      } t_MediaState;

   public:
      QString            SerialNumber;
      QString            Model;
      t_MediaState       SupportsLBA48;
      t_MediaState       SupportsDCO;
      t_MediaState       SupportsHPA;
      t_MediaState       HasHPA;
      t_MediaState       HasDCO;
      unsigned long long HPASize;   // A size of 0
      unsigned long long DCOSize;   // means "unknown"
      unsigned long long RealSize;
};
typedef t_MediaInfo *t_pMediaInfo;


// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_MEDIA_MEMALLOC_FAILED = ERROR_BASE_MEDIA + 1,
   ERROR_MEDIA_IDENTIFY_FAILED
};

#endif


