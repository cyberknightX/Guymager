// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Directory selection dialog
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

#include "dlgmessage.h"
#include "config.h"
#include "qtutil.h"
#include "dlgdirsel.h"
#include "dlgdirsel_private.h"

// -----------------------------
//           Constants
// -----------------------------


// -----------------------------
//           Classes
// -----------------------------

class t_DlgDirSelLocal
{
   public:
      QDirModel   *pModel;
      t_TreeView  *pTree;
      QPushButton *pButtonOk;
      QPushButton *pButtonCancel;
      QString     *pPath;
};

t_DlgDirSel::t_DlgDirSel ()
{
   CHK_EXIT (ERROR_DLGDIRSEL_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised

t_DlgDirSel::t_DlgDirSel (QString *pPath, QWidget *pParent, Qt::WindowFlags Flags)
   :QDialog (pParent, Flags)
{
   QVBoxLayout *pLayout;
   static bool   Initialised = false;
   QModelIndex   IndexCurrentDir;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGDIRSEL_CONSTRUCTOR_NOT_SUPPORTED))
   }

   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)

   pOwn         = new t_DlgDirSelLocal;
   pOwn->pTree  = new t_TreeView (this);
   pOwn->pModel = new QDirModel  (this);
   pOwn->pModel->setReadOnly (false);
   pOwn->pTree ->setModel(pOwn->pModel);
   pOwn->pPath  = pPath;

   IndexCurrentDir = pOwn->pModel->index(*pPath);
   pOwn->pTree->setCurrentIndex (IndexCurrentDir);
   pOwn->pTree->setExpanded     (IndexCurrentDir, true);
   pOwn->pTree->scrollTo        (IndexCurrentDir);
   pOwn->pTree->setColumnWidth  (0, 300);

   setWindowTitle (tr ("Select destination directory", "Dialog title"));
   pLayout = new QVBoxLayout (this);
   setLayout (pLayout);

   pLayout->addWidget (pOwn->pTree);


   // Dialog buttons
   // --------------

   QHBoxLayout *pLayoutButtons = new QHBoxLayout ();
   pLayout->addLayout (pLayoutButtons);

   pOwn->pButtonOk     = new QPushButton (QObject::tr("Ok"    ), this);
   pOwn->pButtonCancel = new QPushButton (QObject::tr("Cancel"), this);
   pLayoutButtons->addWidget (pOwn->pButtonOk    );
   pLayoutButtons->addWidget (pOwn->pButtonCancel);
   pOwn->pButtonOk->setDefault (true);

   CHK_QT_EXIT (connect (pOwn->pButtonOk    , SIGNAL (released()), this, SLOT(SlotAccept())))
   CHK_QT_EXIT (connect (pOwn->pButtonCancel, SIGNAL (released()), this, SLOT(reject    ())))
   CHK_QT_EXIT (connect (pOwn->pTree, SIGNAL(SignalCurrentChanged (const QModelIndex &)),
                         this       , SLOT  (SlotCurrentChanged   (const QModelIndex &))))


   pOwn->pTree->setContextMenuPolicy(Qt::CustomContextMenu);
   CHK_QT_EXIT (connect(pOwn->pTree, SIGNAL(customContextMenuRequested(const QPoint &)),
                        this       , SLOT  (SlotContextMenu           (const QPoint &))))
}

void t_DlgDirSel::SlotCurrentChanged (const QModelIndex &Current)
{
   bool Enable;

   Enable = pOwn->pModel->isDir (Current);
   pOwn->pButtonOk->setEnabled(Enable);
}

void t_DlgDirSel::SlotContextMenu (const QPoint &Position)
{
   QMenu        Menu;
   QAction      ActionNewDir (tr("New directory"), this);
   QAction      ActionRmDir  (tr("Remove directory"), this);
   QAction    *pAction;
   QModelIndex  CurrentIndex;

   CurrentIndex = pOwn->pTree->indexAt (Position);
   if (CurrentIndex.isValid() && pOwn->pModel->isDir (CurrentIndex))
   {
      Menu.addAction (&ActionNewDir);
      Menu.addAction (&ActionRmDir);

      pAction = Menu.exec (pOwn->pTree->mapToGlobal (Position));

      if (pAction == &ActionNewDir)
      {
         pOwn->pModel->mkdir (CurrentIndex, tr("NewDirectory", "Name of the new directory that will be created"));
      }
      else if (pAction == &ActionRmDir)
      {
         bool Success = pOwn->pModel->rmdir (CurrentIndex);
         if (!Success)
            CHK_EXIT (t_DlgMessage::Show ("Cannot be removed", "Only empty directories can be removed", false))
      }
   }
}


void t_DlgDirSel::SlotAccept (void)
{
   QModelIndex IndexCurrent;

   IndexCurrent = pOwn->pTree->currentIndex();
   *(pOwn->pPath) = pOwn->pModel->filePath (IndexCurrent);

   accept();
}


t_DlgDirSel::~t_DlgDirSel ()
{
   delete pOwn->pModel;
   delete pOwn->pTree;
   delete pOwn;
}

