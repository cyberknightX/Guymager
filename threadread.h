// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for reading data.
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

#ifndef THREADREAD_H
#define THREADREAD_H

#include "thread.h"

#ifndef DEVICE_H
   #include "device.h"
#endif

class t_ThreadReadLocal;

class t_ThreadRead: public t_Thread
{
   Q_OBJECT

   public:
      t_ThreadRead ();
      t_ThreadRead (t_pDevice pDevice, bool *pSlowDownRequest);
     ~t_ThreadRead ();

   protected:
      void   run             (void);
      APIRET ThreadReadBlock (t_pFifoBlock &pFifoBlock);

   signals:
      void SignalEnded (t_pDevice pDevice);

   private slots:
      void SlotFinished (void);

   private:
      t_ThreadReadLocal *pOwn;
};

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_THREADREAD_FILESRC_ALREADY_OPEN = ERROR_BASE_THREADREAD + 1,
   ERROR_THREADREAD_FILESRC_NOT_OPEN,
   ERROR_THREADREAD_NO_DATA,
   ERROR_THREADREAD_DEVICE_DISCONNECTED,
   ERROR_THREADREAD_UNEXPECTED_FAILURE,
   ERROR_THREADREAD_CONSTRUCTOR_NOT_SUPPORTED,
   ERROR_THREADREAD_BLOCKSIZE,
   ERROR_THREADREAD_DEVICE_ABORTED,
   ERROR_THREADREAD_LIBEWF_FAILED,
   ERROR_THREADREAD_BAD_FILE_HANDLE
};

#endif

