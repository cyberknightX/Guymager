// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Different message dialogs and boxes we need all the time
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

#ifndef DLGMESSAGE_H
#define DLGMESSAGE_H

#ifndef COMMON_H
   #include "common.h"
#endif

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

class t_DlgMessageLocal;

class t_DlgMessage: public QDialog
{
   Q_OBJECT

   public:
      t_DlgMessage ();
      t_DlgMessage (const QString &Title, QString Message, bool Monospaced=false, QWidget *pParent=nullptr, Qt::WindowFlags Flags=0);
     ~t_DlgMessage ();

      void AdjustGeometry (void);

      static APIRET Show (const QString &Title, const QString &Message, bool Monospaced=false, QWidget *pParent=nullptr, Qt::WindowFlags Flags=0);

   private:
      t_DlgMessageLocal *pOwn;
};

enum
{
   ERROR_DLGMESSAGE_CONSTRUCTOR_NOT_SUPPORTED = ERROR_BASE_DLGMESSAGE + 1,
};


// ----------------------------------------------------------
//  t_MessageBox partially reimplements QMessageBox and uses
//  the font specified by the user's configuration file
// ----------------------------------------------------------

class t_MessageBox: public QMessageBox
{
   private:
      t_MessageBox (QWidget *pParent = 0);
      t_MessageBox (Icon icon, const QString & title, const QString & text, StandardButtons buttons = NoButton, QWidget * parent = 0, Qt::WindowFlags f = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
     ~t_MessageBox ();

      static QMessageBox::StandardButton showNewMessageBox(QWidget *parent, QMessageBox::Icon icon, const QString& title, const QString& text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton);

   public:
      static QMessageBox::StandardButton question    (QWidget *parent, const QString &title, const QString& text, StandardButtons buttons=Ok, StandardButton defaultButton=NoButton);
      static QMessageBox::StandardButton information (QWidget *parent, const QString &title, const QString& text, StandardButtons buttons=Ok, StandardButton defaultButton=NoButton);
      static QMessageBox::StandardButton warning     (QWidget *parent, const QString &title, const QString& text, StandardButtons buttons=Ok, StandardButton defaultButton=NoButton);
      static QMessageBox::StandardButton critical    (QWidget *parent, const QString &title, const QString& text, StandardButtons buttons=Ok, StandardButton defaultButton=NoButton);
};

#endif

