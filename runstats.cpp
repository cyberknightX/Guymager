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
//
// The RunStats module was initially a JSON file generator based on an idea of 
// Erwin van Eijk, who also contributed the initial source code for his periodic 
// JSON updater.

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "common.h"
#include "config.h"
#include "device.h"
#include "runstats.h"

// -------------------------------------
//  Constants and local data structures
// -------------------------------------

static const char *pRunStatsDeviceBlockDelimiter = "%DEVICE_BLOCK%";

class t_RunStatsLocal
{
   public:
      bool           Configured;
      t_pDeviceList pDeviceList;
      QString        Template;
};

// -------------------------------------
//            Main functions
// -------------------------------------

t_RunStats::t_RunStats (t_pDeviceList pDeviceList)
{
   pOwn = new t_RunStatsLocal;

   pOwn->pDeviceList = pDeviceList;
   pOwn->Configured  = (CONFIG(RunStatsInterval) != 0) && strlen(CONFIG(RunStatsTemplateActive))
                                                       && strlen(CONFIG(RunStatsOutput        ));
   ReadTemplate(true);
}

t_RunStats::~t_RunStats (void)
{
   Update (true);
   delete pOwn;
}

bool t_RunStats::IsConfigured (void) const
{
   return pOwn->Configured;
}

void t_RunStats::ReadTemplate (bool LastUpdateOnExit)
{
   char *pTemplateFilename;

   if (!pOwn->Configured)
      return;

   pOwn->Template.clear();
   pTemplateFilename = LastUpdateOnExit ? CONFIG(RunStatsTemplateEnded) : CONFIG(RunStatsTemplateActive);

   QFile File (pTemplateFilename);
   if (!File.open (QIODevice::ReadOnly | QIODevice::Text))
   {
      LOG_INFO ("Can't open runstats template file %s", pTemplateFilename);
      return;
   }

   QTextStream Stream (&File);
   pOwn->Template = Stream.readAll();
   if (pOwn->Template.isEmpty())
      LOG_INFO ("No data found in runstats template file %s", pTemplateFilename);

   File.close();
}

void t_RunStats::Update (bool LastUpdateOnExit)
{
   QFile       File (CONFIG(RunStatsOutput));
   QString     Str;
   QStringList Blocks;
   QString     DeviceBlock;
   QStringList DeviceBlockList;
   static int  UpdateCounter=0;
   t_pDevice  pDevice;
   bool        ProcessDevices;

   if (!pOwn->Configured)
      return;

   if (((UpdateCounter++ % 10) == 0) || LastUpdateOnExit)
      ReadTemplate (LastUpdateOnExit);

   Str = pOwn->Template;

   // Open and process non device related tokens
   // ------------------------------------------
   if (!File.open (QIODevice::WriteOnly))
   {
      LOG_INFO ("Can't open runstats output file %s", CONFIG(RunStatsOutput));
      return;
   }

   QTextStream Out (&File);

   t_Device::ProcessTokenString (Str);  // Resolve all non device related tokens

   // Process device block
   // --------------------
   Blocks = Str.split (pRunStatsDeviceBlockDelimiter);
   ProcessDevices = (Blocks.count() == 3);
   if (!ProcessDevices && (Blocks.count() != 1))
   {
      Str += QString ("\n---Error in RunStats template file: It should contain exactly two device block delimiters (%1) or none at all") .arg(pRunStatsDeviceBlockDelimiter);
      Out << Str;
      return;
   }

   if (ProcessDevices)
   {
       for (int i=0; i<pOwn->pDeviceList->count(); i++)
       {
          pDevice = pOwn->pDeviceList->at(i);
          DeviceBlock = Blocks[1];
          t_Device::ProcessTokenString (DeviceBlock, pDevice);
          DeviceBlockList.append(DeviceBlock);
       }
   }

   // Write RunStats file
   // -------------------
   Out << Blocks[0];
   if (ProcessDevices)
   {
      Out << DeviceBlockList.join("");
      Out << Blocks[2];
   }
}
