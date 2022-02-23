// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Wait dialog
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

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "dlgwait.h"
#include "config.h"
#include "qtutil.h"

// -----------------------------
//           Constants
// -----------------------------


// -----------------------------
//           Classes
// -----------------------------

class t_DlgWaitLocal
{
   public:
      QLabel *pLabel;
};

t_DlgWait::t_DlgWait ()
{
   CHK_EXIT (ERROR_DLGWAIT_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised

t_DlgWait::t_DlgWait (const QString &Title, const QString &Message, QWidget *pParent, Qt::WindowFlags Flags)
   :QDialog (pParent, Flags)
{
   static bool   Initialised = false;
   QVBoxLayout *pLayout;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGWAIT_CONSTRUCTOR_NOT_SUPPORTED))
   }

   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)

   pOwn = new t_DlgWaitLocal;

   pLayout = new QVBoxLayout ();
   pOwn->pLabel = new QLabel (Message, this);

   pLayout->addWidget (pOwn->pLabel);

   setLayout (pLayout);
   setWindowTitle (Title);

//   CHK_QT_EXIT (connect (pOwn->pButtonClose, SIGNAL (released()), this, SLOT(accept())))
}

APIRET t_DlgWait::setLabelText (const QString &Text)
{
   pOwn->pLabel->setText (Text);

   return NO_ERROR;
}

t_DlgWait::~t_DlgWait ()
{
   delete pOwn->pLabel;
   delete pOwn;
}

APIRET t_DlgWait::Show (const QString &Title, const QString &Message)
{
   t_DlgWait *pDlg;

   pDlg = new t_DlgWait (Title, Message);
   pDlg->setModal      (true);
   pDlg->show          ();
   pDlg->exec          ();
   delete pDlg;

   return NO_ERROR;
}

