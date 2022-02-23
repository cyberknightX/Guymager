// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Different Qt utility functions
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

#ifndef QTUTIL_H
#define QTUTIL_H


#define QTUTIL_DEGREES(x) (16*x)
#define QTUTIL_FULL_CIRCLE QTUTIL_DEGREES(360)


#ifdef QDIALOG_H
//   const int QtUtilDefaultDialogFlags = Qt::WStyle_DialogBorder |
//                                        Qt::WStyle_StaysOnTop   |
//                                        Qt::WStyle_Dialog;

   const int QtUtilDefaultDialogFlags = Qt::WindowTitleHint;


// What QMessageBox::information does:
//    : QDialog( parent, name, modal, f | WStyle_Customize | WStyle_DialogBorder | WStyle_Title | WStyle_SysMenu )
#endif

#ifdef QCOLOR_H
   extern const QColor QtUtilColorDefault;
#endif


#ifdef CONFIG_H
   #define QTUTIL_SET_FONT(pWidget, FontObject) \
   {                                            \
      QFont *pFont = CfgGetpFont (FontObject);  \
      if (pFont)                                \
         (pWidget)->setFont(*pFont);            \
   }
#endif


// ------------------------------------
//               Prototypes
// ------------------------------------

#ifdef QWIDGET_H
   APIRET QtUtilSetGeometryCentered (QWidget *pWidget, int Dx, int Dy);
#endif

#if defined QSTRING_H && defined QPAINTER_H && defined QFONT_H
   APIRET QtUtilGetTextAscent  (QPainter const *pPainter, QFont const *pFont, int *pDy);
   APIRET QtUtilGetTextDescent (QPainter const *pPainter, QFont const *pFont, int *pDy);
   APIRET QtUtilGetCharHeight  (QPainter const *pPainter, QFont const *pFont, int *pDy);
   APIRET QtUtilGetLineHeight  (QPainter const *pPainter, QFont const *pFont, int *pDy);
   APIRET QtUtilGetAvgStrWidth (QPainter const *pPainter, QFont const *pFont, int Chars, int *pAvgWidth);
   APIRET QtUtilGetTextSize    (QPainter const *pPainter, QFont const *pFont, const QString &Str, int *pDx, int *pDy);
   APIRET QtUtilGetNumStrWidth (QPainter const *pPainter, QFont const *pFont, int IntLen, int FrcLen , bool Signed, int *pDx);
#ifdef STRUCT_H
   APIRET QtUtilGetNumStrWidth (QPainter const *pPainter, QFont const *pFont, t_pParamUnit pParamUnit, bool Signed, int *pDx);
#endif
#endif

APIRET QtUtilSleep (int MilliSeconds);

#ifdef QLAYOUT_H
   APIRET QtUtilAdjustDefaults (QLayout *pLayout, bool TopLevelLayout=true);
   APIRET QtUtilAdjustLayout   (QLayout *pLayout, bool TopLevelLayout);
#endif
#ifdef QPUSHBUTTON_H
   APIRET QtUtilAdjustPushButton (QPushButton *pWidget);
#endif
#ifdef QLABEL_H
   APIRET QtUtilAdjustLabel      (QLabel      *pWidget);
#endif

APIRET QtUtilProcessCommand (const QString &Command, QString *pStandardOutput=NULL);

APIRET QtUtilInit   (void);
APIRET QtUtilDeInit (void);

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_QTUTIL_DIALOG_STACK_OVERFLOW = ERROR_BASE_QTUTIL + 1,
   ERROR_QTUTIL_DIALOG_STACK_UNDERFLOW,
   ERROR_QTUTIL_DIALOG_INVALID_CUSTOMEVENT,
   ERROR_QTUTIL_DIALOG_UNEXPECTED_EVENT,
   ERROR_QTUTIL_INVALID_MESSAGEBOX_TYPE,
   ERROR_QTUTIL_UNFREED_MEMORY,
   ERROR_QTUTIL_COMMAND_TIMEOUT
};

#endif


