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

#ifndef ERROR_H
#define ERROR_H

// -----------------------
//       Exit codes
// -----------------------

static const int EXIT_NO_ERROR        = 0;
static const int EXIT_AUTOEXIT        = 1;
static const int EXIT_INTERNAL_ERROR  = 10;
static const int EXIT_QT_FATAL        = 11;
static const int EXIT_SIGNAL_RECEIVED = 12;

// -----------------------
//      Prototypes
// -----------------------

void ErrorExit (int ExitCode = EXIT_INTERNAL_ERROR);
void ErrorPopupExit (const char *pFileName, const char *pFunctionName, int LineNr, APIRET ec, const char *pMsg);

APIRET ErrorInit   (void);
APIRET ErrorDeInit (void);

// -----------------------
//  Error handling macros
// -----------------------


#define CHK_QT(Func)                                              \
   {                                                              \
      if (!(Func))                                                \
      {                                                           \
         (void)ToolErrorCheck (__FFL__, ERROR_QT_UNSUCCESSFUL);   \
         return ERROR_QT_UNSUCCESSFUL;                            \
      }                                                           \
   }


#define CHK_QT_POPUP(Func)                                        \
   {                                                              \
      if (!(Func))                                                \
         ErrorPopupExit (__FFL__, ERROR_QT_UNSUCCESSFUL, "Qt returned FALSE");  \
   }


#define CHK_QT_EXIT(Func)                                         \
   {                                                              \
      if (!(Func))                                                \
      {                                                           \
         (void)ToolErrorCheck (__FFL__, ERROR_QT_UNSUCCESSFUL);   \
         exit (EXIT_QT_FATAL);                                    \
      }                                                           \
   }



#define CHK_EXIT(Func)                     \
{                                          \
   APIRET ec;                              \
   /*lint -save -e506 -e774 -e1551*/       \
   if ((ec = Func) != NO_ERROR)            \
   {                                       \
      (void)ToolErrorCheck (__FFL__, ec);  \
      ErrorExit ();                        \
   }                                       \
   /*lint -restore */                      \
}


#define CHK_EXIT_POPUP_MSG(Func,pMsg)         \
/*lint -save -e506 -e774*/   /*Warning 506: Constant value Boolean; Info 774: Boolean within 'if' always evaluates to True */ \
   {                                          \
      APIRET ec;                              \
      if ((ec = Func) != NO_ERROR)            \
         ErrorPopupExit (__FFL__, ec, pMsg);  \
   }                                          \
/*lint -restore*/


#define CHK_EXIT_POPUP(Func)                  \
   CHK_POPUP_MSG(Func,NULL)                   \


#define CHK_EXIT_POPUP_CONST(ec)              \
      ErrorPopupExit (__FFL__, ec, NULL);



#endif
