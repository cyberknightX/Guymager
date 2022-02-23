// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The info field area
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

#ifndef INFOFIELD_H
#define INFOFIELD_H

#ifndef COMMON_H
   #include "common.h"
#endif

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#ifndef DEVICE_H
  #include "device.h"
#endif

class t_InfoFieldLocal;

class t_InfoField: public QFrame
{
   Q_OBJECT

   public:
      t_InfoField ();
      t_InfoField (QWidget *pParent, t_pDeviceList pDeviceList);
     ~t_InfoField ();

   static QString GetHashText (bool MD5, bool SHA1, bool SHA256);

   public slots:
      void SlotShowInfo (t_pDevice pDevice);

   private:
      t_InfoFieldLocal *pOwn;
};

typedef t_InfoField *t_pInfoField;


// ------------------------------------
//             Error codes
// ------------------------------------

   #ifdef MODULES_H
      enum
      {
         ERROR_INFOFIELD_CONSTRUCTOR_NOT_SUPPORTED = ERROR_BASE_INFOFIELD + 1,
      };
   #endif

#endif
