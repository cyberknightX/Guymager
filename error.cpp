// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Error handling utilities
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

#include "common.h"

#include <stdarg.h>
#include <stdlib.h>

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "qtutil.h"

// -----------------------
//     Constants
// -----------------------

static const int EXIT_SLEEP_BEFORE_EXIT = 3000; // ms

// -----------------------
//     Local variables
// -----------------------

static int ErrorPopupCounter = 0;


// -----------------------
//       Functions
// -----------------------

void ErrorExit (int ExitCode)
{
   LOG_ERROR ("Exiting GUYMAGER with code %d", ExitCode)
   printf ("\n\n");

   (void) QtUtilSleep (EXIT_SLEEP_BEFORE_EXIT); // Quite often, the last messages of the slave cannot be
   exit (ExitCode);
}


void ErrorPopupExit (const char *pFileName, const char *pFunctionName, int LineNr, APIRET ec, const char *pMsg)
{
   const char *pErr;
   QString      MsgTxt;

   if (ErrorPopupCounter < 3)   // If there are already several windows, then they are most probably generated by a timer and we risk
   {                    // to fill up the screen without being able to click them all away. So, we simply exit here.
      ErrorPopupCounter++;
      (void) ToolErrorCheck (pFileName, pFunctionName, LineNr, ec);
      pErr = ToolErrorMessage(ec);
      if (pMsg)
           MsgTxt = QString(pMsg) + QString("\n") + QString(pErr);
      else MsgTxt = pErr;
      (void) QMessageBox::critical (nullptr, "Internal error", MsgTxt);
   }
   ErrorExit ();
}

APIRET ErrorInit (void)
{
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QT_UNSUCCESSFUL))
   return NO_ERROR;
}

APIRET ErrorDeInit (void)
{
   return NO_ERROR;
}


