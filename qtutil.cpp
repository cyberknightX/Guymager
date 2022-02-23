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

#include "common.h"

#include <algorithm>

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "config.h"
#include "qtutil.h"

// ------------------------------------
//              Constants
// ------------------------------------


const QColor QtUtilColorDefault; // Accessible externally as well, see qtutil.h. This color is an invalid color (see description of the
                                 // QColor default construtor) and is interpreted in the QtUtil functions as the default color (gray for
                                 // a button background, for instance; see QtUtilSetButtonColor for an example on how it is used)

static const float QTUTIL_GEOMETRY_MAXFACTOR_X = 0.95f;
static const float QTUTIL_GEOMETRY_MAXFACTOR_Y = 0.95f;

static const int QTUTIL_WAIT_PROCESS_MAX    = 60000;

// ------------------------------------
//          Type definitions
// ------------------------------------

//typedef struct
//{
//} t_QtUtilLocal;

// ------------------------------------
//          Global variables
// ------------------------------------

//static t_QtUtilLocal QtUtilLocal;

// ------------------------------------
//              Functions
// ------------------------------------

APIRET QtUtilSetGeometryCentered (QWidget *pWidget, int Dx, int Dy)
{
   QWidget *pParent;
   int       X, Y;
   int       ScreenDx, ScreenDy;

   ScreenDx = QApplication::desktop()->width ();
   ScreenDy = QApplication::desktop()->height();

   Dx = GETMIN (Dx, (int)(ScreenDx * QTUTIL_GEOMETRY_MAXFACTOR_X)); // Take care that the window size never gets bigger
   Dy = GETMIN (Dy, (int)(ScreenDy * QTUTIL_GEOMETRY_MAXFACTOR_Y)); // than the screen minus a certain border.

   if (pWidget->windowFlags() & Qt::Dialog)
        pParent = nullptr;
   else pParent = pWidget->parentWidget();

   if (pParent == nullptr)
   {
      X = (ScreenDx - Dx) / 2;
      Y = (ScreenDy - Dy) / 2;
   }
   else
   {
      X  = (pParent->width () - Dx) / 2;
      Y  = (pParent->height() - Dy) / 2;
   }
   pWidget->setGeometry(X, Y, Dx, Dy);

   return NO_ERROR;
}

// ------------------------------------
//             Font metrics
// ------------------------------------

static APIRET QtUtilGetFontMetrics (QPainter const *pPainter, QFont const *pFont, QFontMetrics **ppFontMetrics)
{
   if (pFont)
        *ppFontMetrics = new QFontMetrics(*pFont);
   else *ppFontMetrics = new QFontMetrics(pPainter->font());

   return NO_ERROR;
}

APIRET QtUtilGetTextSize (QPainter const *pPainter, QFont const *pFont, const QString &Str, int *pDx, int *pDy)
{
   QFontMetrics *pFontMetrics;
   QRect          Rect;

   CHK (QtUtilGetFontMetrics (pPainter, pFont, &pFontMetrics))
   Rect = pFontMetrics->boundingRect(Str);

   if (pDx) *pDx = Rect.width ();
   if (pDy) *pDy = Rect.height();

   delete pFontMetrics;

   return NO_ERROR;
}

APIRET QtUtilGetCharHeight (QPainter const *pPainter, QFont const *pFont, int *pDy)
{
   QFontMetrics *pFontMetrics;

   CHK (QtUtilGetFontMetrics (pPainter, pFont, &pFontMetrics))
   *pDy = pFontMetrics->ascent();
   delete pFontMetrics;

   return NO_ERROR;
}

APIRET QtUtilGetLineHeight (QPainter const *pPainter, QFont const *pFont, int *pDy)
{
   QFontMetrics *pFontMetrics;

   CHK (QtUtilGetFontMetrics (pPainter, pFont, &pFontMetrics))
   *pDy = pFontMetrics->height();
   delete pFontMetrics;

   return NO_ERROR;
}

APIRET QtUtilGetTextAscent (QPainter const *pPainter, QFont const *pFont, int *pDy)
{
   QFontMetrics *pFontMetrics;

   CHK (QtUtilGetFontMetrics (pPainter, pFont, &pFontMetrics))
   *pDy = pFontMetrics->ascent();
   delete pFontMetrics;

   return NO_ERROR;
}

APIRET QtUtilGetTextDescent (QPainter const *pPainter, QFont const *pFont, int *pDy)
{
   QFontMetrics *pFontMetrics;

   CHK (QtUtilGetFontMetrics (pPainter, pFont, &pFontMetrics))
   *pDy = pFontMetrics->descent();
   delete pFontMetrics;

   return NO_ERROR;
}

APIRET QtUtilGetNumStrWidth (QPainter const *pPainter, QFont const *pFont, int IntLen, int FrcLen, bool Signed, int *pDx)
{
   QString Str;
   int     Digits;

   Digits = IntLen + FrcLen;
   if (FrcLen > 0) Digits++;       // +1 for the ','
   if (Signed    ) Digits++;

   Str.sprintf ("%0*.*f", Digits, FrcLen, 0.0);
   Str.replace ('.', ',');

   CHK (QtUtilGetTextSize (pPainter, pFont, Str, pDx, nullptr))

   return NO_ERROR;
}

APIRET QtUtilGetAvgStrWidth (QPainter const *pPainter, QFont const *pFont, int Chars, int *pAvgWidth)
{
   QString AvgChar;
   int     AvgCharWidth;

   AvgChar = "N";    // Let it be the average character...
   CHK (QtUtilGetTextSize (pPainter, pFont, AvgChar, &AvgCharWidth, nullptr))
   *pAvgWidth = Chars * AvgCharWidth;

   return NO_ERROR;
}


// ------------------------------------
//             List view
// ------------------------------------

// t_QtUtilListView::t_QtUtilListView (QWidget *pParent, const char *pName, WFlags Flags)
//    :QListView (pParent, pName, Flags)
// {
// }
//
// t_QtUtilListView::~t_QtUtilListView (void)
// {
// }
//
// APIRET t_QtUtilListView::AdjustColumnWidths (void)
// {
//    QTimer::singleShot (0, this, SLOT(SlotAdjustColumnWidths()));
//    return NO_ERROR;
// }
//
// void t_QtUtilListView::SlotAdjustColumnWidths (void)
// {
//    int i;
//    for (i=0; i<columns(); i++)
//       adjustColumn (i);
//    triggerUpdate();
// }

// ------------------------------------
//               Sleep
// ------------------------------------

class QtUtilThread: public QThread
{
   public:
      static APIRET Sleep (int MilliSeconds)
      {
         msleep ((unsigned long)MilliSeconds);
         return NO_ERROR;
      }
};

APIRET QtUtilSleep (int MilliSeconds)
{
   CHK (QtUtilThread::Sleep(MilliSeconds))

   return NO_ERROR;
}


// ------------------------------------
//         Default widget setting
// ------------------------------------

#ifdef OLDOLD
APIRET QtUtilAdjustPushButton (QPushButton *pWidget)
{
   pWidget->setFont (CONFIG_FONT (FONTOBJECT_DIALOGDEFAULT));
   return NO_ERROR;
}

APIRET QtUtilAdjustLabel (QLabel *pWidget)
{
   pWidget->setFont (CONFIG_FONT (FONTOBJECT_DIALOGDEFAULT));
   return NO_ERROR;
}


APIRET QtUtilAdjustLayout (QLayout *pLayout, bool TopLevelLayout)
{
   if (TopLevelLayout)
        pLayout->setMargin (15);
   else pLayout->setMargin ( 0);
   pLayout->setSpacing (15);

   return NO_ERROR;
}

APIRET QtUtilAdjustDefaults (QLayout *pLayout, bool TopLevelLayout)
{
   QLayoutItem     *pLayoutItem;
   QWidget         *pChildWidget;
   QLayout         *pChildLayout;
   int               i = 0;

   CHK (QtUtilAdjustLayout (pLayout, TopLevelLayout))

   while ((pLayoutItem = pLayout->itemAt(i)) != 0)
   {
      pChildWidget = pLayoutItem->widget();
      pChildLayout = pLayoutItem->layout();
      if (pChildWidget != nullptr)
      {
//         if      (pChildWidget->inherits ("QPushButton")) CHK (QtUtilAdjustPushButton ((QPushButton*) pChildWidget))
//         else if (pChildWidget->inherits ("QLabel"     )) CHK (QtUtilAdjustLabel      ((QLabel     *) pChildWidget))
         if      (pChildWidget->inherits ("QPushButton")) CHK (QtUtilAdjustPushButton (dynamic_cast<QPushButton*>(pChildWidget)))
         else if (pChildWidget->inherits ("QLabel"     )) CHK (QtUtilAdjustLabel      (dynamic_cast<QLabel     *>(pChildWidget)))
      }
      else if (pChildLayout != nullptr)
      {
         CHK (QtUtilAdjustDefaults (pChildLayout, false)) // recursive call for processing sub-layouts
      }
   }

   return NO_ERROR;
}
#endif
// ------------------------------------
//          External calls
// ------------------------------------

APIRET QtUtilProcessCommand (const QString &Command, QString *pStandardOutput)
{
   QProcess    Process;
   QStringList SplittedCommand;

   QStringList Arguments;
   QString     StrState;
   QString     Cmd = Command;
   QString     LastArg;
   bool        qtrc;

   Process.setProcessChannelMode (QProcess::MergedChannels); // Merge stderr and stdout

   // Split up the whole command string into command and arguments
   // ------------------------------------------------------------
   SplittedCommand = Command.split (' ');
   Cmd = SplittedCommand.takeFirst();                       // The first part is the command
   while (!SplittedCommand.isEmpty())                       // Then come the arguments
   {
      if (SplittedCommand.first()[0] != '"')
           Arguments.append (SplittedCommand.takeFirst());
      else break;                                           // If an argument starts with " we assume that comes one big last argument
   }
   if (!SplittedCommand.isEmpty())
   {
      LastArg = SplittedCommand.join (" ");                 // Put all the rest together
      LastArg = LastArg.right (LastArg.length()-1);         // Take away the leading "
      if (LastArg[LastArg.length()-1] == '"')               // If there is a trailing ",
         LastArg = LastArg.left (LastArg.length()-1);       // take it away as well
      Arguments.append (LastArg);
   }                                    // The whole handling is not really clean. We try to copy bash's behaviour, very roughly. It
                                        // should work for all commands of the form   bash -c "xxxx yy zzz xx"
   // Run the command
   // ---------------
   Process.start (Cmd, Arguments);

   StrState = "WaitForStarted";
   qtrc = Process.waitForStarted  (QTUTIL_WAIT_PROCESS_MAX);
   if (qtrc)
   {
      StrState = "WaitForFinished";
      Process.closeWriteChannel ();
      qtrc = Process.waitForFinished (QTUTIL_WAIT_PROCESS_MAX);
   }
   if (qtrc)
   {
//    StandardOutput  = Process.readAllStandardOutput();
//    StandardOutput  = Process.readAll();
      if (pStandardOutput)
         *pStandardOutput = QString::fromUtf8 (Process.readAll().constData());

      StrState = "Done";
   }
   else
   {
      LOG_INFO ("Killing command because of timeout or non-existence [%s] in state %s", QSTR_TO_PSZ(Command), QSTR_TO_PSZ(StrState))
      Process.kill();
      return ERROR_QTUTIL_COMMAND_TIMEOUT;
   }

   return NO_ERROR;
}

//#include <sys/types.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <wait.h>
//
//APIRET QtUtilProcessCommand_C (const QString &Command, QString &StandardOutput)
//{
//   FILE *pStream;
//   int    Ch;
//   pid_t  Pid;
//   int    ChildExitCode;
//   int    PipeArr[2];      /* This holds the fd for the input & output of the pipe */
//
//   // Setup communication pipe
//   // ------------------------
//   if (pipe(PipeArr))
//      CHK (ERROR_QTUTIL_PIPE_ERROR)
//
//   // Attempt to fork
//   // ---------------
//   Pid = fork();
//   if (Pid == -1)
//      CHK (ERROR_QTUTIL_FORK_FAILED)
//
//   if (Pid)  // Parent process
//   {
//      close (PipeArr[STDOUT_FILENO]);                      // Close unused side of pipe (in side)
//
//      pStream = fdopen (PipeArr[STDIN_FILENO], "r");
//      if (pStream == nullptr)
//         CHK (ERROR_QTUTIL_STREAM_OPEN_FAILED)
//
//      while ((Ch = fgetc (pStream)) != EOF)
//         StandardOutput += QChar(Ch);
//
//      fclose (pStream);
//      waitpid (Pid, &ChildExitCode, STDIN_FILENO);         // Wait for child process to end
//   }
//   else // Child process
//   {
//      dup2  (commpipe[STDOUT_FILENO],STDOUT_FILENO);       // Replace stdout with out side of the pipe
//      close (commpipe[STDIN_FILENO ]);                     // Close unused side of pipe (out side)
//
//      execl ("/bin/bash", "/bin/bash", "--noprofile", "--norc", "--posix", "-c", QSTR_TO_PSZ(Command), nullptr);    // Replace the child fork with a new process
//      _exit (1);
//   }
//
//   return NO_ERROR;
//}

// ------------------------------------
//       Module initialisation
// ------------------------------------

APIRET QtUtilInit (void)
{
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_DIALOG_STACK_OVERFLOW     ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_DIALOG_STACK_UNDERFLOW    ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_DIALOG_INVALID_CUSTOMEVENT))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_DIALOG_UNEXPECTED_EVENT   ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_INVALID_MESSAGEBOX_TYPE   ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_UNFREED_MEMORY            ))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_QTUTIL_COMMAND_TIMEOUT           ))

   return NO_ERROR;
}

APIRET QtUtilDeInit (void)
{
   return NO_ERROR;
}

