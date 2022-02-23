// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

// Main window table column names
// ------------------------------

#define MAINWINDOW_COLUMN_SerialNr       QT_TRANSLATE_NOOP("t_DeviceListModel", "SerialNr"           )
#define MAINWINDOW_COLUMN_LinuxDevice    QT_TRANSLATE_NOOP("t_DeviceListModel", "LinuxDevice"        )
#define MAINWINDOW_COLUMN_Model          QT_TRANSLATE_NOOP("t_DeviceListModel", "Model"              )
#define MAINWINDOW_COLUMN_NativePath     QT_TRANSLATE_NOOP("t_DeviceListModel", "NativePath"         )
#define MAINWINDOW_COLUMN_ByPath         QT_TRANSLATE_NOOP("t_DeviceListModel", "ByPath"             )
#define MAINWINDOW_COLUMN_Interface      QT_TRANSLATE_NOOP("t_DeviceListModel", "Interface"          )
#define MAINWINDOW_COLUMN_State          QT_TRANSLATE_NOOP("t_DeviceListModel", "State"              )
#define MAINWINDOW_COLUMN_AddStateInfo   QT_TRANSLATE_NOOP("t_DeviceListModel", "AdditionalStateInfo")
#define MAINWINDOW_COLUMN_Size           QT_TRANSLATE_NOOP("t_DeviceListModel", "Size"               )
#define MAINWINDOW_COLUMN_HiddenAreas    QT_TRANSLATE_NOOP("t_DeviceListModel", "HiddenAreas"        )
#define MAINWINDOW_COLUMN_BadSectors     QT_TRANSLATE_NOOP("t_DeviceListModel", "BadSectors"         )
#define MAINWINDOW_COLUMN_Progress       QT_TRANSLATE_NOOP("t_DeviceListModel", "Progress"           )
#define MAINWINDOW_COLUMN_AverageSpeed   QT_TRANSLATE_NOOP("t_DeviceListModel", "AverageSpeed"       )
#define MAINWINDOW_COLUMN_TimeRemaining  QT_TRANSLATE_NOOP("t_DeviceListModel", "TimeRemaining"      )
#define MAINWINDOW_COLUMN_FifoUsage      QT_TRANSLATE_NOOP("t_DeviceListModel", "FifoUsage"          )
#define MAINWINDOW_COLUMN_SectorSizeLog  QT_TRANSLATE_NOOP("t_DeviceListModel", "SectorSizeLog"      )
#define MAINWINDOW_COLUMN_SectorSizePhys QT_TRANSLATE_NOOP("t_DeviceListModel", "SectorSizePhys"     )
#define MAINWINDOW_COLUMN_CurrentSpeed   QT_TRANSLATE_NOOP("t_DeviceListModel", "CurrentSpeed"       )
#define MAINWINDOW_COLUMN_UserField                                             "UserField" // No automatic Qt translation for this one, see t_MainTranslator
#define MAINWINDOW_COLUMN_Examiner       QT_TRANSLATE_NOOP("t_DeviceListModel", "Examiner"           )

// Main window classes
// -------------------

class t_MainWindowLocal;

class t_MainWindow: public QMainWindow
{
   Q_OBJECT

   public:
      t_MainWindow (void);
      t_MainWindow (t_pDeviceList pDeviceList, QWidget *pParent = 0, Qt::WindowFlags Flags = 0);
     ~t_MainWindow ();

   APIRET RemoveSpecialDevice (t_pDevice pDevice);
   bool   AutoExit (void);

   private:
      APIRET CreateMenu             (void);
      APIRET CheckDisconnectTimeout (void);

   protected:
      void closeEvent (QCloseEvent *pEvent);

   public slots:
      void SlotRescan           (void);
      
   private slots:
      void SlotAddSpecialDevice (void);
      void SlotDebug            (void);
      void SlotAboutGuymager    (void);
      void SlotAboutQt          (void);
      void SlotAutoExit         (void);
      void SlotScanStarted      (void);
      void SlotScanFinished     (t_pDeviceList);
      void SlotRefresh          (void);
      void SlotUpdateRunStats   (void);

   signals:
      void SignalAutoExit       (void);

   private:
      t_MainWindowLocal *pOwn;
};

#ifdef DEVICELISTMODEL_H
   typedef struct
   {
      const char                      *pName;
      t_DeviceListModel::t_pGetDataFn  pGetDataFn;
      int                               DisplayType;
   } t_MainWindowColumn, *t_pMainWindowColumn;

   t_pMainWindowColumn MainWindowGetColumn (const char *pName);
#endif

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_MAINWINDOW_CONSTRUCTOR_NOT_SUPPORTED = ERROR_BASE_MAINWINDOW + 1,
   ERROR_MAINWINDOW_INVALID_COLUMN,
   ERROR_MAINWINDOW_INVALID_DATATYPE,
   ERROR_MAINWINDOW_DEVICE_NOT_FOUND,
   ERROR_MAINWINDOW_UNKNOWN_COLUMN
};


#endif

