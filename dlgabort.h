// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Dialog that is opened when aborting an acquisition
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

#ifndef DLGABORT_H
#define DLGABORT_H

#ifndef COMMON_H
   #include "common.h"
#endif

#if (QT_VERSION > 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#ifndef DEVICE_H
  #include "device.h"
#endif

class t_DlgAbortLocal;

class t_DlgAbort: public QDialog
{
   Q_OBJECT

   public:
      t_DlgAbort ();
      t_DlgAbort (t_pcDevice pDevice, QWidget *pParent=nullptr, Qt::WindowFlags Flags=0);
     ~t_DlgAbort ();

      static APIRET Show (t_pcDevice pDevice, bool &Abort, bool &Delete);

   private:
      t_DlgAbortLocal *pOwn;
};

enum
{
   ERROR_DLGABORT_CONSTRUCTOR_NOT_SUPPORTED = ERROR_BASE_DLGABORT + 1,
   ERROR_DLGABORT_INVALID_CONFIG_SETTING
};

#endif

