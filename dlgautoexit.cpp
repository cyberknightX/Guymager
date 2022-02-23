// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Auto exit dialog
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

#include "dlgautoexit.h"
#include "config.h"
#include "qtutil.h"

// -----------------------------
//           Constants
// -----------------------------


// -----------------------------
//           Classes
// -----------------------------

class t_DlgAutoExitLocal
{
   public:
      QLabel *pLabel;
      int      Seconds;
};

t_DlgAutoExit::t_DlgAutoExit ()
{
   CHK_EXIT (ERROR_DLGAUTOEXIT_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised

t_DlgAutoExit::t_DlgAutoExit (QWidget *pParent, Qt::WindowFlags Flags)
   :QDialog (pParent, Flags)
{
   static bool        Initialised = false;
   QVBoxLayout      *pMainLayout;
   QDialogButtonBox *pButtonBox;
   QPushButton      *pButtonAbort;
   QPushButton      *pButtonExit;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGAUTOEXIT_CONSTRUCTOR_NOT_SUPPORTED))
   }

   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)

   pOwn = new t_DlgAutoExitLocal;
   pOwn->Seconds = CONFIG (AutoExitCountdown);

   pMainLayout = new QVBoxLayout ();
   pOwn->pLabel = new QLabel (this);
   pMainLayout->addWidget (pOwn->pLabel);

   pButtonAbort = new QPushButton (tr("Abort"), this);
   pButtonExit  = new QPushButton (tr("Exit now"), this);
   pButtonAbort->setDefault (true);

   pButtonBox = new QDialogButtonBox (this);
   pButtonBox->addButton(pButtonAbort, QDialogButtonBox::RejectRole);
   pButtonBox->addButton(pButtonExit , QDialogButtonBox::AcceptRole);
   pMainLayout->addWidget (pButtonBox);

   connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

   SlotCountdown();

   setLayout (pMainLayout);
   setWindowTitle ("test");

   QTimer *pTimer = new QTimer(this);
   connect(pTimer, SIGNAL(timeout()), this, SLOT(SlotCountdown()));
   pTimer->start(1000);

//   CHK_QT_EXIT (connect (pOwn->pButtonClose, SIGNAL (released()), this, SLOT(accept())))
}

t_DlgAutoExit::~t_DlgAutoExit ()
{
   delete pOwn;
}

void t_DlgAutoExit::SlotCountdown (void)
{
   pOwn->pLabel->setText (tr("All acquisitions have ended.\nGuymager will exit automatically in %1 seconds.") .arg (pOwn->Seconds));
   if (pOwn->Seconds == 0)
      accept();
   pOwn->Seconds--;
}

APIRET t_DlgAutoExit::Show (bool *pAutoExit)
{
   t_DlgAutoExit *pDlg;
   int          Result;

   pDlg = new t_DlgAutoExit (nullptr);
   pDlg->setModal      (true);
   pDlg->show          ();
   Result = pDlg->exec ();
   delete pDlg;
   *pAutoExit = (Result == QDialog::Accepted);

   return NO_ERROR;
}

