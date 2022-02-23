// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Private definitions of acquisition dialog
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

#ifndef DLGACQUIRE_PRIVATE_H
#define DLGACQUIRE_PRIVATE_H

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#ifndef COMMON_H
   #include "common.h"
#endif

#ifndef DLGACQUIRE_H
   #include "dlgacquire.h"
#endif

class   t_DlgAcquireLineEdit;
typedef t_DlgAcquireLineEdit *t_pDlgAcquireLineEdit;

class t_DlgAcquireLineEdit: public QLineEdit
{
   Q_OBJECT

   public:
      t_DlgAcquireLineEdit(void)
      {
         CHK_EXIT (ERROR_DLGACQUIRE_CONSTRUCTOR_NOT_SUPPORTED)
      }

      t_DlgAcquireLineEdit (QWidget *pParent, const QString &Name)
         :QLineEdit (pParent)                                      //lint !e578: Declaration of symbol 'Name' hides ...
      {
         this->Name = Name;
         CHK_QT_EXIT (connect (this, SIGNAL (textEdited     (const QString &)),
                               this, SLOT   (SlotTextEdited (const QString &))))
         setMinimumWidth(fontMetrics().averageCharWidth()*8);
      }

     ~t_DlgAcquireLineEdit() {}

     void TextUpdated (void)
     {
        emit SignalTextEdited (this, text());
     }

   public:
      QString Name;

   private slots:
      void SlotTextEdited (const QString &Text)
      {
         emit SignalTextEdited (this, Text);
      }

   signals:
      void SignalTextEdited (t_DlgAcquireLineEdit *pDlgAcquireLineEdit, const QString &Text);
};

#endif

