// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Acquisition dialog
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

#ifndef DLGACQUIRE_H
#define DLGACQUIRE_H

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

#ifndef CONFIG_H
   class   t_CfgEntryMode;
   class   t_CfgDlgAcquireField;
   typedef t_CfgDlgAcquireField *t_pCfgDlgAcquireField;
#endif

class t_DlgAcquireLineEdit;
class t_DlgAcquireLocal;

class t_DlgAcquire: public QDialog
{
   Q_OBJECT

   public:
      enum {ButtonCancel = QDialog::Rejected, // QDialog::Rejected is returned when clicking on the close button on the window broder, so use the same code for cancel
            ButtonOk     = 100,               // Start with a value that doesn't collide woth QDialog's standard return codes
            ButtonDuplicate,
            ButtonBack};

   public:
      t_DlgAcquire ();
      t_DlgAcquire (t_pDevice pDevice, bool Clone, t_pDeviceList pDeviceList, bool Duplicate = false, QWidget *pParent=nullptr, Qt::WindowFlags Flags=0, bool ForceLastEnteredValues=false, t_pAcquisition pAcquisition1=nullptr);
     ~t_DlgAcquire ();

      APIRET GetParameters (t_Acquisition &Acquisition, bool RememberLastUsedValues=true);
   private:
      t_CfgEntryMode EntryMode            (t_pCfgDlgAcquireField pField);
      APIRET         AddField             (t_pDevice pDevice, t_pCfgDlgAcquireField pField, QGridLayout *pLayout, int *pRow, int *pCol, bool ForceLastEnteredValues);
      APIRET         AdjustPathAndFilename(QString &Path, QString &Filename);
      APIRET         CheckWriteAccess     (t_pcDevice pDevice, const QString &Path, const QString &Filename, bool &Ok);
      APIRET         CreateDeviceTable    (t_pDeviceList pDeviceList, t_pDevice pDeviceSrc, QTableWidget **ppTable);
      APIRET         InsertDeviceTableRow (QTableWidget *pTable, int Row, t_pDeviceList pDeviceList, t_pDevice pDevSrc, t_pDevice pDev);
      void           Accept               (bool Duplicate);

   private slots:
      void UpdateHashState     (int State=0);
      void UpdateDialogState   (const QString & NewText = QString());
      void UpdateFieldState    (void);
      void UpdateFileSplitState(void);
      void SlotBrowse          (void);
      void SlotOk              (void);
      void SlotDuplicate       (void);
      void SlotBack            (void);
      void SlotCancel          (void);
      void SlotTextEdited      (t_DlgAcquireLineEdit *pLineEdit, const QString &NewVal);
      void SlotDeviceTableSelectionChanged (void);

   private:
      t_DlgAcquireLocal *pOwn;
};

enum
{
   ERROR_DLGACQUIRE_CONSTRUCTOR_NOT_SUPPORTED = ERROR_BASE_DLGACQUIRE + 1,
   ERROR_DLGACQUIRE_UNKNOWN_FILEDIALOG_SIZE,
   ERROR_DLGACQUIRE_INVALID_ENTRYMODE,
   ERROR_DLGACQUIRE_INVALID_FORMAT,
   ERROR_DLGACQUIRE_INVALID_SELECTION,
   ERROR_DLGACQUIRE_UNKNOWN_COLUMN
};

#endif

