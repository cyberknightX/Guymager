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

#ifndef THREADSCAN_H
#define THREADSCAN_H

#include <QObject>  //lint !e537 Repeated include

#include "thread.h"

#ifndef DEVICE_H
   #include "device.h"
#endif

#ifndef COMMON_H
   #include "common.h"
#endif

class t_ThreadScan;

class t_ThreadScanWorker: public QObject
{
   Q_OBJECT

   public:
      t_ThreadScanWorker (t_ThreadScan *pThreadScan);

   public slots:
      virtual void SlotRescan (void) = 0;

   signals:
      void SignalScanFinished (t_pDeviceList);
      void SignalScanStarted  (void);

   protected:
      t_ThreadScan *poThreadScan;
};

class t_ThreadScanWorkerPartedLocal;
class t_ThreadScanWorkerParted: public t_ThreadScanWorker
{
   Q_OBJECT

   public:
      t_ThreadScanWorkerParted (t_ThreadScan *pThreadScan, APIRET &rc);
     ~t_ThreadScanWorkerParted (void);

   public slots:
      void SlotRescan (void);

   private:
      t_ThreadScanWorkerPartedLocal *pOwn;
};


class t_ThreadScanWorkerUdevLocal;
class t_ThreadScanWorkerUdev: public t_ThreadScanWorker
{
   Q_OBJECT

   public:
      t_ThreadScanWorkerUdev (t_ThreadScan *pThreadScan, APIRET &rc);
     ~t_ThreadScanWorkerUdev (void);

   public slots:
      void SlotRescan (void);

   private:
      t_ThreadScanWorkerUdevLocal *pOwn;
};


class t_ThreadScanWorkerHALLocal;
class t_ThreadScanWorkerHAL: public t_ThreadScanWorker
{
   Q_OBJECT

   public:
      t_ThreadScanWorkerHAL (t_ThreadScan *pThreadScan, APIRET &rc);
     ~t_ThreadScanWorkerHAL (void);

   private:
      QList<QVariant> CallMethod        (const QString &Device, const QString &Method, const QString &Argument);
      QVariant        CallMethodSingle  (const QString &Device, const QString &Method, const QString &Argument);
      APIRET          GetProperty       (const QString &Device, const QString &Property, QList<QVariant> &VarList);
      APIRET          GetPropertySingle (const QString &Device, const QString &Property, QVariant        &Var    );
      bool            PropertyContains  (const QString &Device, const QString &Property, const QString   &Str    );

   public slots:
      void SlotRescan (void);

   private:
      t_ThreadScanWorkerHALLocal *pOwn;
};

class t_ThreadScanWorkerDevKitLocal;
class t_ThreadScanWorkerDevKit: public t_ThreadScanWorker
{
   Q_OBJECT

   public:
      t_ThreadScanWorkerDevKit (t_ThreadScan *pThreadScan, APIRET &rc);
     ~t_ThreadScanWorkerDevKit (void);

   private:
      QList<QVariant> CallMethod        (const QString &Device, const QString &Method, const QString &Argument);
      APIRET          GetProperty       (const QString &Device, const QString &Property, QVariant &Var);

   public slots:
      void SlotRescan (void);

   private:
      t_ThreadScanWorkerDevKitLocal *pOwn;
};


class t_ThreadScan: public t_Thread
{
   Q_OBJECT

   public:
      t_ThreadScan (void);
      APIRET Start (t_ThreadScanWorker **ppWorker);  // Return ptr to worker, so calling fn may emit signals to it
      APIRET Stop  ();

   protected:
      void run (void);

   private:
      t_ThreadScanWorker **ppoWorker;
      APIRET                 oWorkerRc;
};

enum
{
   ERROR_THREADSCAN_NOT_STARTED = ERROR_BASE_THREADSCAN,
   ERROR_THREADSCAN_NOT_STOPPED,
   ERROR_THREADSCAN_EXITCODE_NONZERO,
   ERROR_THREADSCAN_PROCESS_NOTSTARTED,
   ERROR_THREADSCAN_PROCESS_NOTFINISHED,
   ERROR_THREADSCAN_LIBPARTED_NOTWORKING,
   ERROR_THREADSCAN_LIBUDEV_NOTWORKING,
   ERROR_THREADSCAN_DBUSHAL_NOTWORKING,
   ERROR_THREADSCAN_DBUSDEVKIT_NOTWORKING,
   ERROR_THREADSCAN_PROPERTY_NONEXISTENT,
   ERROR_THREADSCAN_CALLED_FROM_WRONG_THREAD,
   ERROR_THREADSCAN_INVALID_SCAN_METHOD,
   ERROR_THREADSCAN_UDEV_FAILURE
};

#endif
