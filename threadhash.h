// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for calculating hashes
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

#ifndef THREADHASH_H
#define THREADHASH_H

#include "thread.h"

#ifndef DEVICE_H
  #include "device.h"
#endif

class t_ThreadHashLocal;

class t_ThreadHash: public t_Thread
{
   Q_OBJECT

   public:
      t_ThreadHash ();
      t_ThreadHash (t_pDevice pDevice);
     ~t_ThreadHash ();

   protected:
      void run (void);

   signals:
      void SignalEnded (t_pDevice pDevice);

   private slots:
      void SlotFinished (void);

   private:
      t_ThreadHashLocal *pOwn;
};

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_THREADHASH_CONSTRUCTOR_NOT_SUPPORTED = ERROR_BASE_THREADHASH + 1,
   ERROR_THREADHASH_LIBEWF_FAILED
};

#endif

