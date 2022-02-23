// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         RunStats provides information about Guymager's state to
//                  others
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

#ifndef RUNSTATS_H
#define RUNSTATS_H

class t_RunStatsLocal;

class t_RunStats
{
   public:
      t_RunStats (t_pDeviceList pDeviceList);
     ~t_RunStats (void);

     bool IsConfigured (void) const;
     void Update       (bool LastUpdateOnExit=false);

   private:
      void ReadTemplate (bool LastUpdateOnExit);

   private:
      t_RunStatsLocal *pOwn;
};

typedef t_RunStats *t_pRunStats;

#endif
