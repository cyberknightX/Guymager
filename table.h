// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The table widget (central widget of the application)
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

#ifndef TABLE_H
#define TABLE_H

#ifndef COMMON_H
   #include "common.h"
#endif

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "common.h"
#include "device.h"
#include "threadwrite.h"

class t_TableLocal;
class t_MainWindow;

class t_Table: public QTableView
{
   Q_OBJECT

   public:
      t_Table ();
      t_Table (QWidget *pParent, t_MainWindow *pMainWindow, t_pDeviceList pDeviceList);
     ~t_Table ();

      APIRET AbortAllAcquisitions (void);
      APIRET GetDeviceUnderCursor (t_pDevice &pDevice);

   private:
      APIRET ShowDeviceInfo            (t_pcDevice pDevice);
      APIRET ShowAcquisitionDialog     (t_pDevice  pDevice, bool Clone);
      APIRET StartAcquisition          (t_pDevice  pDevice);
      APIRET AbortAcquisition0         (t_pDevice  pDevice);
      APIRET AbortAcquisition          (t_pDevice  pDevice);
      APIRET InfoAcquisitionStart      (t_pAcquisition pAcquisition);
      APIRET InfoAcquisitionBadSectors (t_pAcquisition pAcquisition, bool Verify);
      APIRET InfoAcquisitionEnd0       (t_pAcquisition pAcquisition);
      APIRET InfoAcquisitionEnd        (t_pAcquisition pAcquisition);
      APIRET ThreadWriteFinished       (t_pDevice  pDevice, t_pThreadWrite *ppThreadWrite);
      APIRET WakeWaitingThreads        (t_pDevice  pDevice);
      APIRET FinaliseThreadStructs     (t_pDevice  pDevice);
      APIRET RescheduleJobs            (void);
      APIRET ContextMenu               (const QPoint &GlobalPos);

   protected:
      void contextMenuEvent      (QContextMenuEvent *pEvent);
      void mouseDoubleClickEvent (QMouseEvent       *pEvent);

   private slots:
      void SlotMouseClick             (const QModelIndex & index);
      void SlotThreadReadFinished     (t_pDevice pDevice);
      void SlotThreadWrite1Finished   (t_pDevice pDevice);
      void SlotThreadWrite2Finished   (t_pDevice pDevice);
      void SlotThreadHashFinished     (t_pDevice pDevice);
      void SlotThreadCompressFinished (t_pDevice pDevice, int ThreadNr);
      void SlotThreadWriteWakeThreads (t_pDevice pDevice);
      void SlotInvalidFreeHandle      (t_pDevice pDevice);

  signals:
      void SignalDeviceSelected       (t_pDevice pDevice);
      void SignalAllAcquisitionsEnded (void);

   private:
      t_TableLocal *pOwn;
};

typedef t_Table *t_pTable;


// ------------------------------------
//             Error codes
// ------------------------------------

   #ifdef MODULES_H
      enum
      {
         ERROR_TABLE_INVALID_ACTION = ERROR_BASE_TABLE + 1,
         ERROR_TABLE_THREADREAD_ALREADY_RUNNING,
         ERROR_TABLE_THREADHASH_ALREADY_RUNNING,
         ERROR_TABLE_THREADWRITE_ALREADY_RUNNING,
         ERROR_TABLE_THREADCOMPRESS_ALREADY_RUNNING,
         ERROR_TABLE_THREADREAD_DOESNT_EXIT,
         ERROR_TABLE_FIFO_EXISTS,
         ERROR_TABLE_CONSTRUCTOR_NOT_SUPPORTED,
         ERROR_TABLE_INVALID_FORMAT,
         ERROR_TABLE_INVALID_DIALOG_CODE,
         ERROR_TABLE_INVALID_FREE_HANDLE
      };
   #endif

#endif
