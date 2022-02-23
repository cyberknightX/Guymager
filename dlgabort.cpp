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

#include "common.h"

#include "dlgabort.h"
#include "config.h"
#include "qtutil.h"

// -----------------------------
//           Constants
// -----------------------------


// -----------------------------
//           Classes
// -----------------------------

class t_DlgAbortLocal
{
   public:
      QCheckBox *pCheckBoxDelete;
};

t_DlgAbort::t_DlgAbort (void)
{
   CHK_EXIT (ERROR_DLGABORT_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised

t_DlgAbort::t_DlgAbort (t_pcDevice pDevice, QWidget *pParent, Qt::WindowFlags Flags)
   :QDialog (pParent, Flags)
{
   static bool Initialised = false;
   QString     Text;
   bool        Acquisition, Tick;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGABORT_CONSTRUCTOR_NOT_SUPPORTED))
      Initialised = true;
   }

   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)

   pOwn = new t_DlgAbortLocal;
   pOwn->pCheckBoxDelete = nullptr;

   Acquisition = ((pDevice->GetState() == t_Device::Acquire)       ||
                  (pDevice->GetState() == t_Device::Launch)        ||
                  (pDevice->GetState() == t_Device::LaunchPaused)  ||
                  (pDevice->GetState() == t_Device::AcquirePaused) ||
                  (pDevice->GetState() == t_Device::Queued));
   if (Acquisition)
        Text = tr ("Do you want to abort the acquisition of %1?" ) .arg(pDevice->LinuxDevice);
   else Text = tr ("Do you want to abort the verification of %1?") .arg(pDevice->LinuxDevice);

   QVBoxLayout *pMainLayout  = new QVBoxLayout ();
   QHBoxLayout *pButtonLayout= new QHBoxLayout ();
   QLabel      *pLabel       = new QLabel      (Text, this);
   QPushButton *pButtonOk    = new QPushButton (QObject::tr("Ok"    ), this);
   QPushButton *pButtonCancel= new QPushButton (QObject::tr("Cancel"), this);
   if (!pDevice->Acquisition1.Clone)
   {      
      Tick = false;
      switch (CONFIG(DeleteAbortedImageFiles))
      {
         case CFG_BOOLEANAUTO_TRUE:  Tick = true;        break;
         case CFG_BOOLEANAUTO_FALSE: Tick = false;       break;
         case CFG_BOOLEANAUTO_AUTO:  Tick = Acquisition; break;
         default: CHK_EXIT (ERROR_DLGABORT_INVALID_CONFIG_SETTING)
      }
      pOwn->pCheckBoxDelete = new QCheckBox (tr("Delete image files already created"), this);
      pOwn->pCheckBoxDelete->setChecked (Tick);
   }

   pMainLayout->addWidget   (pLabel);
   if (pOwn->pCheckBoxDelete)
      pMainLayout->addWidget   (pOwn->pCheckBoxDelete);
   pMainLayout->addLayout   (pButtonLayout);
   pButtonLayout->addWidget (pButtonOk);
   pButtonLayout->addWidget (pButtonCancel);

   setLayout (pMainLayout);
   setWindowTitle (tr("Abort?", "Dialog title"));

   CHK_QT_EXIT (connect (pButtonOk    , SIGNAL (released()), this, SLOT(accept())))
   CHK_QT_EXIT (connect (pButtonCancel, SIGNAL (released()), this, SLOT(reject())))
}


t_DlgAbort::~t_DlgAbort ()
{
   delete pOwn;
}


APIRET t_DlgAbort::Show (t_pcDevice pDevice, bool &Abort, bool &Delete)
{
   t_DlgAbort *pDlg;
   int          Result;

   pDlg = new t_DlgAbort (pDevice);
   pDlg->setModal  (true);
   Result = pDlg->exec();
   Abort  = (Result == QDialog::Accepted);
   if (pDlg->pOwn->pCheckBoxDelete)
        Delete = (Abort && pDlg->pOwn->pCheckBoxDelete->isChecked());
   else Delete = false;
   delete pDlg;

   return NO_ERROR;
}

