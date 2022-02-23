// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Information about the acquisition, creates the info file
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

#ifndef INFO_H
#define INFO_H

#include <QObject>

#ifndef COMMON_H
   #include "common.h"
#endif


class t_InfoLocal;

class t_Info: public QObject
{
   Q_OBJECT

   public:
      t_Info (t_pcAcquisition pAcquisition=nullptr);
     ~t_Info (void);

//      void   SetAcquisiiton  (t_pcAcquisition pAcquisition);
      APIRET Create     (void);
      APIRET vWrite     (const char *pFormat, va_list pArguments);
      APIRET Write      (const char *pFormat, ...) __attribute__ ((format (printf, 2, 3))); // Non-static class functions have implicit this argument, thus we start counting the parameters for __attribute__ at 2, not 1 (see GNU C-Lib Function-Atttributes.html)
      APIRET Write      (const QString &Text);
      APIRET WriteLn    (const QString &Text=QString());
      APIRET Title      (const QString &Text);
      APIRET InitTable  (const QString &ColSep, const QString &ColSepReplace);
      APIRET AddRow     (const QString &Row);
      APIRET WriteTable (void);

      APIRET WriteDeviceInfo (void);

      static APIRET GetDeviceInfo (t_pcDevice pDevice, bool RichText, QString &Info);

   private:
      QString FullFileName (void);

   private:
      t_InfoLocal *pOwn;
};

typedef t_Info *t_pInfo;


// ------------------------------------
//             Error codes
// ------------------------------------

   #ifdef MODULES_H
      enum
      {
         ERROR_INFO_FILE_CREATE_FAILED  = ERROR_BASE_INFO + 1,
         ERROR_INFO_FILE_OPEN_FAILED,
         ERROR_INFO_FILE_WRITE_FAILED,
         ERROR_INFO_FILE_CLOSE_FAILED,
      };
   #endif

#endif

