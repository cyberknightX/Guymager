// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for writing data.
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

#ifndef THREADWRITE_H
#define THREADWRITE_H

#include "thread.h"

#ifndef DEVICE_H
  #include "device.h"
#endif


class t_ThreadWriteLocal;

class t_ThreadWrite: public t_Thread
{
   Q_OBJECT

   public:
      t_ThreadWrite ();
      t_ThreadWrite (t_pAcquisition pAcquisition, t_pFifo *ppFifo, bool *pSlowDownRequest);
     ~t_ThreadWrite ();

      APIRET GetpFileHandle    (void **ppHandle);
      APIRET ReleaseFileHandle (void);
      int    Instance          (void);
   private:
      APIRET GetpFileHandle0   (void **ppHandle);

   protected:
      void run (void);

   private:
      APIRET DeleteImageFiles (bool AlsoDeleteInfoFile);

   signals:
      void SignalEnded        (t_pDevice pDevice);
      void SignalFreeMyHandle (t_pDevice pDevice);

   private slots:
      void SlotFinished (void);

   private:
      t_ThreadWriteLocal *pOwn;
};

typedef t_ThreadWrite *t_pThreadWrite;

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_THREADWRITE_OPEN_FAILED = ERROR_BASE_THREADWRITE + 1,
   ERROR_THREADWRITE_WRITE_FAILED,
   ERROR_THREADWRITE_VERIFY_FAILED,
   ERROR_THREADWRITE_CLOSE_FAILED,
   ERROR_THREADWRITE_NOT_OPENED,
   ERROR_THREADWRITE_INVALID_FORMAT,
   ERROR_THREADWRITE_CONSTRUCTOR_NOT_SUPPORTED,
   ERROR_THREADWRITE_WRONG_BLOCK,
   ERROR_THREADWRITE_OUT_OF_SEQUENCE_BLOCK,
   ERROR_THREADWRITE_HANDLE_NOT_YET_AVAILABLE,
   ERROR_THREADWRITE_HANDLE_TIMEOUT,
   ERROR_THREADWRITE_LIBEWF_FAILED,
   ERROR_THREADWRITE_MALLOC_FAILED,
   ERROR_THREADWRITE_OSVERSION_QUERY_FAILED
};

#endif
