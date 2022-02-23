// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The main programm function, responsible for intialisation,
//                  main window creation, entering the message loop and
//                  deinitialisation.
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

#include <mcheck.h>
#include <limits.h>
#include <signal.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/socket.h>

#include <QApplication>
#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "common.h"

#include "toolconstants.h"
#include "toolsignal.h"
#include "toolsysinfo.h"
#include "toolcfg.h"

#include "ewf.h"

#include "compileinfo.h"
#include "config.h"
#include "device.h"
#include "qtutil.h"
#include "util.h"
#include "hash.h"
#include "aaff.h"
#include "aewf.h"
#include "dlgmessage.h"
#include "mainwindow.h"
#include "main.h"
#include "main_private.h"


// ------------------------------------
//                Macros
// ------------------------------------

#define MAIN_ENVVAR_MALLOC_TRACE "MALLOC_TRACE"

// ------------------------------------
//              Constants
// ------------------------------------

static const uint MAIN_MAX_ERROR_CODES         = 1000;
static const uint  MAIN_SHOW_SPLASH_TIME_EVENT =  300;  // ms
static const uint  MAIN_PROCESSEVENTS_MAXTIME  = 1000;

enum
{
   ERROR_MAIN_UNKNOWN_STARTUP_SIZE = ERROR_BASE_MAIN + 1,
   ERROR_MAIN_INVALID_WINDOW_SIZE,
   ERROR_MAIN_INVALID_NUMBERSTYLE,
   ERROR_MAIN_LIBEWF_VERSION_MISMATCH,
   ERROR_MAIN_SIGACTION_FAILED,
   ERROR_MAIN_SOCKETPAIR_NOT_CREATED
};

#define MAIN_SPLASH_PIXMAP             "splash.png"
#define MAIN_LANGUAGE_FILENAME         "guymager_"
#define MAIN_LANGUAGE_FILENAME_DEFAULT "guymager_en"
#define MAIN_LANGUAGE_FILENAME_QT      "qt_"
#define MAIN_DEFAULT_MISCFILES_DIR     "/usr/share/guymager/"
#define MAIN_DEFAULT_LANGUAGE_DIR_QT   "/usr/share/qt5/translations/"
#ifndef SPLASH_DIR
   #define SPLASH_DIR                     MAIN_DEFAULT_MISCFILES_DIR
#endif

// ------------------------------------
//   Application-wide global variables
// ------------------------------------


// ------------------------------------
//          Type definitions
// ------------------------------------

typedef struct                // Objects that exist only once for the whole application
{
   QApplication                *pApp;
   QLocale                     *pNumberLocale; // The QLocale for number conversions
   t_Log                       *pLog;
   t_MainWindow                *pMainWindow;
   t_pDeviceList                pDeviceList;
   bool                          CriticalInitDone;
   char                        *pCommandLine;
   t_MainSystemSignalForwarder *pSystemSignalForwarder;
} t_MainLocal;

static t_MainLocal MainLocal;

// ------------------------------------
//       System signal forwarding
// ------------------------------------

// See http://doc.qt.io/qt-5/unix-signals.html for explanantions
// Code of t_MainSystemSignalForwarder mainly has been copied from that web page

int t_MainSystemSignalForwarder::oArrFileDescHup[2];

#pragma GCC diagnostic push
#ifdef Q_CREATOR_RUN
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#endif

t_MainSystemSignalForwarder::t_MainSystemSignalForwarder (QObject *parent)
   :QObject(parent)
{
   struct sigaction SigActionHup;
	
   if (::socketpair(AF_UNIX, SOCK_STREAM, 0, oArrFileDescHup))
      CHK_EXIT (ERROR_MAIN_SOCKETPAIR_NOT_CREATED)

   poSocketNotifierHup = new QSocketNotifier (oArrFileDescHup[1], QSocketNotifier::Read, this);
   connect (poSocketNotifierHup, SIGNAL(activated(int)), this, SLOT(SlotHup()));
   
   sigemptyset (&SigActionHup.sa_mask);
   SigActionHup.sa_handler = SignalHandlerHup;
   SigActionHup.sa_flags   = 0;
   SigActionHup.sa_flags  |= SA_RESTART;
   if (sigaction(SIGHUP, &SigActionHup, nullptr))
      CHK_EXIT (ERROR_MAIN_SIGACTION_FAILED)
}      
#pragma GCC diagnostic pop

t_MainSystemSignalForwarder::~t_MainSystemSignalForwarder ()
{
}

void t_MainSystemSignalForwarder::SignalHandlerHup (int Signal) // Unix signal handler
{
   if (Signal != SIGHUP)
   {
      LOG_ERROR ("Unexpected signal %d", Signal)
      return;
   }
   
   char Ch = '1';
   if (::write (oArrFileDescHup[0], &Ch, sizeof(Ch)) != 1)
      LOG_ERROR ("Write to socket failed")
}
    
void t_MainSystemSignalForwarder::SlotHup()
{
   char Ch;

   poSocketNotifierHup->setEnabled(false);
   if (::read(oArrFileDescHup[1], &Ch, sizeof(Ch)) != 1)
      LOG_ERROR ("Read from socket failed")

//   LOG_INFO ("Signal HUP received (rescan)")
   MainLocal.pMainWindow->SlotRescan();

   poSocketNotifierHup->setEnabled(true);
}
      
// ------------------------------------
//          Error handling
// ------------------------------------

#if (QT_VERSION >= 0x050000)
   static void MainLogQtMessage (QtMsgType MsgType, const QMessageLogContext &/*Context*/, const QString &Message)
   {
      const char *pMsgType;

      switch (MsgType)
      {
         case QtDebugMsg:    pMsgType = "Qt debug";    break;
         case QtWarningMsg:  pMsgType = "Qt warning";  break;
         case QtCriticalMsg: pMsgType = "Qt critical"; break;
         case QtFatalMsg:    pMsgType = "Qt fatal";    break;
         default:            pMsgType = "Qt message of unknown type:"; break;
      }
//      t_Log::Entry (t_Log::Info, Context.file, Context.function, Context.line, "%s: %s", pMsgType, QSTR_TO_PSZ(Message));  // Context.file/function/line contain non-interesting
//      printf ("\n%s %s %d - %s: %s", Context.file, Context.function, Context.line, pMsgType, QSTR_TO_PSZ(Message));        // info about Qt source code

      LOG_INFO ("%s: %s", pMsgType, QSTR_TO_PSZ(Message));
      printf ("\n%s: %s\n", pMsgType, QSTR_TO_PSZ(Message));

      if (MsgType == QtFatalMsg)
         ErrorExit (EXIT_QT_FATAL);
   }
#else
   static void MainLogQtMessage (QtMsgType MsgType, const char *pMsg)
   {
      const char *pMsgType;

      switch (MsgType)
      {
         case QtDebugMsg:   pMsgType = "Qt debug";   break;
         case QtWarningMsg: pMsgType = "Qt warning"; break;
         case QtFatalMsg:   pMsgType = "Qt fatal";   break;
         default:           pMsgType = "Qt message of unknown type:"; break;
      }
      LOG_INFO ("%s: %s", pMsgType, pMsg)
      printf ("\n%s: %s\n", pMsgType, pMsg);

      if (MsgType == QtFatalMsg)
         ErrorExit (EXIT_QT_FATAL);
   }
#endif




// MainSignalHandler: Called by ToolSignal whenever a signal is caught
static void MainSignalHandler (int Signal)
{
   printf ("\n");
   printf ("%c[1;37;41m", ASCII_ESC);  // white on red
   printf ("Signal no. %d received: %s", Signal, strsignal(Signal));
   printf ("%c[0m"      , ASCII_ESC);  // standard

   if (MainLocal.CriticalInitDone)
      LOG_ERROR ("Signal no. %d received: %s", Signal, strsignal(Signal));

   if (Signal != SIGSEGV)
      ErrorExit (EXIT_SIGNAL_RECEIVED);
}

// MainSignalLog: Called by ToolSignal for doing log output
static void MainSignalLog (bool Error, unsigned long long /*ThreadNr*/, const char *pFileName, const char *pFunction, int LineNr, const char *pFormat, va_list pArguments)
{
   t_Log::t_Level Level;

   if (Error)
        Level = t_Log::Error;
   else Level = t_Log::Info;

   t_Log::vEntry (Level, pFileName, pFunction, LineNr, pFormat, pArguments);
}

static APIRET MainWindowAdjustSize (t_MainWindow *pMainWindow)
{
   int ScreenDx, ScreenDy;

   switch (CONFIG(StartupSize))
   {
      case CFG_STARTUPSIZE_STANDARD  : pMainWindow->show();           break;
      case CFG_STARTUPSIZE_FULLSCREEN: pMainWindow->showFullScreen(); break;
      case CFG_STARTUPSIZE_MAXIMIZED : pMainWindow->showMaximized (); break;
      case CFG_STARTUPSIZE_MANUAL    : pMainWindow->setGeometry (CONFIG (StartupSizeManualX ), CONFIG (StartupSizeManualY),
                                                                 CONFIG (StartupSizeManualDx), CONFIG (StartupSizeManualDy));
                                       pMainWindow->show();           break;
      default                        : CHK_CONST (ERROR_MAIN_UNKNOWN_STARTUP_SIZE)
   }

   ScreenDx = QApplication::desktop()->width ();
   ScreenDy = QApplication::desktop()->height();

   if ((MainLocal.pMainWindow->width () > ScreenDx) ||
       (MainLocal.pMainWindow->height() > ScreenDy))
   {
      LOG_INFO ("The window manager gave the application a size that's too big: %d x %d (maximum size is %d x %d)",
                  MainLocal.pMainWindow->width (), MainLocal.pMainWindow->height(), ScreenDx, ScreenDy)
//      CHK_CONST (ERROR_MAIN_INVALID_WINDOW_SIZE)
   }

   return NO_ERROR;
}


// ------------------------------------
//          Configuration log
// ------------------------------------

static void MainCfgLog (t_pcchar pFileName, t_pcchar pFunction, t_int LineNr, t_pcchar pFormat, va_list pArguments)
{
   static bool ConsoleLog = false;
   va_list     pArgumentsCopy;

   if (!ConsoleLog)
        ConsoleLog = (strstr (pFormat, "Configuration error"       ) != nullptr);
   else ConsoleLog = (strstr (pFormat, "TOOLCFG_ERROR_CONFIG_ERROR") == nullptr);

   __va_copy (pArgumentsCopy, pArguments);
   if (ConsoleLog)
   {
      printf ("\n");
      vprintf (pFormat, pArguments);
   }
   t_Log::vEntryInfo (pFileName, pFunction, LineNr, pFormat, pArgumentsCopy);
   va_end (pArgumentsCopy);
}

// ------------------------------------
//        Translator extension
// ------------------------------------

class t_MainTranslator : public QTranslator    // Has been extended initially to support configuration file driven
{                                              // translation of text UserField, might be used for others as well.
   public:
      t_MainTranslator (QObject *pParent=nullptr) : QTranslator (pParent) {}

      void Insert (const QString &Source, const QString &Translation)
      {
         (void) oDict.insert (Source, Translation);
      }

      QString translate (const char *pContext, const char *pSourceText, const char *pDisambiguation = nullptr, int n = -1) const
      {
         QString Source(pSourceText);

         if (oDict.contains (Source))
              return oDict.value (Source);
         else return QTranslator::translate (pContext, pSourceText, pDisambiguation, n);
      }

      bool isEmpty (void) const
      {
         return oDict.isEmpty();
      }

   private :
      QHash<QString, QString> oDict;
};


// ------------------------------------
//           Main program
// ------------------------------------

APIRET MainPocessEvents (void)
{
   QCoreApplication::processEvents (QEventLoop::AllEvents, MAIN_PROCESSEVENTS_MAXTIME);
   return NO_ERROR;
}

static void ShowSplashText (QSplashScreen *pSplashScreen, QString Text)
{
   if (pSplashScreen)
   {
      pSplashScreen->showMessage (Text, Qt::AlignHCenter | Qt::AlignBottom, Qt::white);
      QApplication::processEvents ();
      CHK_EXIT (QtUtilSleep (MAIN_SHOW_SPLASH_TIME_EVENT))
      QApplication::processEvents ();
   }
}

static APIRET MainGo (int argc, char *argv[], bool *pAutoExit)
{
   QSplashScreen *pSplashScreen = nullptr;
   QPixmap       *pSplashPixmap;
   QString         Uname;
   const char    *pLogFileName;
   const char    *pCfgFileName;
   bool            DefaultUsed;
   APIRET          Err = NO_ERROR;
   APIRET          rc;
   int             RetCode;
   const char    *pLibGuyToolsVersionInstalled;
   quint64         FifoAllocs;
   quint64         FifoFrees;
   qint64          FifoRemaining;
   qint64          FifoAllocated;
   uid_t           UserIdReal;
   uid_t           UserIdEffective;
   struct passwd *pUser; // Attention, structure only exists once! Do not define multiple pointers to it
   QMessageBox::StandardButton  Button;


   // Initialise Error / Cfg / Log, open log file
   // -------------------------------------------

   MainLocal.CriticalInitDone = false;
   CHK_EXIT (ToolErrorInit              (MAIN_MAX_ERROR_CODES))
   CHK_EXIT (ErrorInit())
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE   (ERROR_MAIN_UNKNOWN_STARTUP_SIZE   ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE   (ERROR_MAIN_INVALID_WINDOW_SIZE    ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE   (ERROR_MAIN_INVALID_NUMBERSTYLE    ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE   (ERROR_MAIN_LIBEWF_VERSION_MISMATCH))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE   (ERROR_MAIN_SIGACTION_FAILED       ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE   (ERROR_MAIN_SOCKETPAIR_NOT_CREATED ))

   CHK_EXIT (ToolErrorSetLogFn          (&t_Log::vEntryError))
   CHK_EXIT (ToolCfgInit                (argc, argv))
   CHK_EXIT (CfgGetLogFileName          (&pLogFileName, &DefaultUsed))
   if (DefaultUsed)
      printf ("\nUsing default log file name %s", pLogFileName);
   MainLocal.pLog = new t_Log (pLogFileName, Err);
   CHK_EXIT (Err)
   LOG_INFO ("=================================================== Starting GUYMAGER ===================================================")
   LOG_INFO ("Version: %s", pCompileInfoVersion)
   LOG_INFO ("Version timestamp: %s", pCompileInfoTimestampChangelog)
   LOG_INFO ("Compile timestamp: %s", pCompileInfoTimestampBuild)
   CHK_EXIT (ToolSysInfoUname (Uname))
   LOG_INFO ("System: %s", QSTR_TO_PSZ(Uname))

   t_Log::GetLibGuyToolsVersion (&pLibGuyToolsVersionInstalled);
   LOG_INFO ("Libguytools version installed on this PC: %s", pLibGuyToolsVersionInstalled)
   #if (ENABLE_LIBEWF)
      LOG_INFO ("Libewf      version installed on this PC: %s", (const char *) libewf_get_version())
   #else
      LOG_INFO ("Compiled without libewf support")
   #endif

   unsigned long long Bytes = UtilGetInstalledRAM ();
   LOG_INFO ("Total amount of memory installed: %0.1f MB", Bytes/(1024.0*1024.0))

   // Initialise QApplication
   // -----------------------
   QApplication::setDesktopSettingsAware (false);
   QApplication::setColorSpec (QApplication::ManyColor);
   MainLocal.pApp = new QApplication(argc, argv); // Create QApplication here, so functions below may use
                                                  // Qt functions, for instance for building pixmaps, fonts, ...
   #if (QT_VERSION >= 0x050000)
      qInstallMessageHandler (MainLogQtMessage);
   #else
      qInstallMsgHandler (MainLogQtMessage);
   #endif
   CHK (ToolSysInfoInit ())

   // Check Cfg file
   // --------------
   int      AdjustedArgc;
   char **ppAdjustedArgv;

   AdjustedArgc = QApplication::arguments().count();                          // Ask Qt for command line argument list; the list returned already has been
   ppAdjustedArgv = (char**) malloc ((size_t)AdjustedArgc * sizeof(char *));  // cleaned so that Qt specific command line parameters (such as -style=plastique)
   for (int i=0; i<AdjustedArgc; i++)                                         // no longer figure in it.
      ppAdjustedArgv[i] = strdup (QSTR_TO_PSZ(QApplication::arguments()[i]));

   CHK (ToolCfgUseAdjustedCommandLine (AdjustedArgc, ppAdjustedArgv))
   CHK (ToolCfgSetLogFn (&MainCfgLog))
   CHK (CfgInit())
   rc = CfgGetCfgFileName (&pCfgFileName, &DefaultUsed);
   if (rc == ERROR_CFG_ONLY_TEMPLATE_GENERATED)
   {
      printf ("\nTemplate configuration file has been written");
      return NO_ERROR;
   }
   if (DefaultUsed)
      printf ("\nUsing default cfg file name %s", pCfgFileName);

   // Show splashscreen and read cfg file
   // -----------------------------------
   QString SplashFile = MAIN_SPLASH_PIXMAP;

   #ifdef SPLASH_DIR
      QString SplashDir = SPLASH_DIR;

      if (!SplashDir.endsWith ("/"))
         SplashDir += "/";
      SplashFile = SplashDir + SplashFile;
   #endif

   pSplashPixmap = new QPixmap (SplashFile);
   if (pSplashPixmap->isNull())
   {
      LOG_INFO ("Pixmap %s not found, no splashscreen will be shown.", MAIN_SPLASH_PIXMAP)
   }
   else
   {
      pSplashScreen = new QSplashScreen (*pSplashPixmap);
      pSplashScreen->show();
      ShowSplashText (pSplashScreen, "Reading configuration");
   }
   CHK (CfgReadConfiguration (pCfgFileName))

   // Language selection
   // ------------------
   ShowSplashText (pSplashScreen, "Setting up language");
   QTranslator      TranslatorQt;
   QTranslator      TranslatorGuymager;
   t_MainTranslator TranslatorDynamic;
   QString          LangNameQt= QString(MAIN_LANGUAGE_FILENAME_QT) + CONFIG(Language);
   QString          LangName  = QString(MAIN_LANGUAGE_FILENAME   ) + CONFIG(Language);
   QString          LangDirQt = MAIN_DEFAULT_LANGUAGE_DIR_QT;
   QString          LangDir   = MAIN_DEFAULT_MISCFILES_DIR;
   QString          TryName, TryDir;
   bool             Found;

   #ifdef LANGUAGE_DIR_QT
      LangDirQt = LANGUAGE_DIR_QT;
   #endif
   #ifdef LANGUAGE_DIR
      LangDir = LANGUAGE_DIR;
   #endif

   if (!LangDirQt.endsWith ("/")) LangDirQt += "/";
   if (!LangDir  .endsWith ("/")) LangDir   += "/";

   // Select correct Qt language file. We only try once. If this does not
   // succeed, Qt automatically uses its english language file.

   if (TranslatorQt.load (LangNameQt, LangDirQt))
   {
      LOG_INFO ("Using Qt language file %s in %s", QSTR_TO_PSZ(LangNameQt), QSTR_TO_PSZ(LangDirQt))
      MainLocal.pApp->installTranslator(&TranslatorQt);
   }
   else
   {
      LOG_INFO ("Qt language file %s not found in %s, using no Qt language file.", QSTR_TO_PSZ(LangNameQt), QSTR_TO_PSZ(LangDirQt))
   }

   // Load Guymager language file. We try everal times with different directories and names.
   TryName = LangName;
   TryDir  = LangDir;

   Found = TranslatorGuymager.load (TryName, TryDir);
   if (!Found)
   {
      LOG_INFO ("Language file %s not found in %s", QSTR_TO_PSZ(TryName), QSTR_TO_PSZ(TryDir))
      TryDir = ".";
      Found = TranslatorGuymager.load (TryName, TryDir);
   }
   if (!Found)
   {
      LOG_INFO ("Language file %s not found in %s", QSTR_TO_PSZ(TryName), QSTR_TO_PSZ(TryDir))
      TryDir  = LangDir;
      TryName = MAIN_LANGUAGE_FILENAME_DEFAULT;
      Found = TranslatorGuymager.load (TryName, TryDir);
   }
   if (!Found)
   {
      LOG_INFO ("Language file %s not found in %s", QSTR_TO_PSZ(TryName), QSTR_TO_PSZ(TryDir))
      TryDir = ".";
      Found = TranslatorGuymager.load (TryName, TryDir);
   }
   if (Found)
   {
      LOG_INFO ("Using language file %s in %s.", QSTR_TO_PSZ(TryName), QSTR_TO_PSZ(TryDir))
      MainLocal.pApp->installTranslator (&TranslatorGuymager);
   }
   else
   {
      LOG_INFO ("Language file %s not found in %s. Not using any language file.", QSTR_TO_PSZ(TryName), QSTR_TO_PSZ(TryDir))
   }

   if (strlen (CONFIG(UserFieldName)))
      TranslatorDynamic.Insert (MAINWINDOW_COLUMN_UserField   , CONFIG(UserFieldName));
   if (strlen (CONFIG(AdditionalStateInfoName)))
      TranslatorDynamic.Insert (MAINWINDOW_COLUMN_AddStateInfo, CONFIG(AdditionalStateInfoName));
   MainLocal.pApp->installTranslator (&TranslatorDynamic);  // TranslatorDynamic is installed last, so it will be queried first
                                                            // and its translation will beat all others if ever it has a hit.

   // Check real and effective user ID
   // --------------------------------
   UserIdReal      = getuid ();
   UserIdEffective = geteuid();
   pUser = getpwuid (UserIdReal);      LOG_INFO ("User real     : %d - %s", UserIdReal     , pUser ? pUser->pw_name : "name not found")
   pUser = getpwuid (UserIdEffective); LOG_INFO ("User effective: %d - %s", UserIdEffective, pUser ? pUser->pw_name : "name not found")

   if (UserIdEffective == 0)
   {
      LOG_INFO ("Running with root rights")
   }
   else
   {
      if (CONFIG(CheckRootRights))
      {
         LOG_INFO ("Running with non-root rights, asking user...")
         Button = t_MessageBox::question (nullptr, QObject::tr ("Missing root rights", "Dialog title"),
                                                   QObject::tr ("Guymager needs to be started with root rights in order to perform acquisitions. "
                                                                "You are not root and thus won't be able to acquire devices.\nContinue anyway?"),
                                                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
         if (Button == QMessageBox::Yes)
         {
            LOG_INFO ("User continues anyway")
         }
         else
         {
            LOG_INFO ("User doesn't want to continue. Exiting now.")
            return NO_ERROR;
         }
      }
      else
      {
         LOG_INFO ("Running with non-root rights, continuing without asking user (CheckRootRights is off)")
      }
   }

   // Install system signal handlers
   // ------------------------------

   MainLocal.pSystemSignalForwarder = new t_MainSystemSignalForwarder();

   if (CONFIG (SignalHandling))
      CHK_EXIT (ToolSignalInit (MainSignalLog, MainSignalHandler, nullptr))

   // Critical initialisation phase done, CFG, ERROR and LOG operational now
   // ----------------------------------------------------------------------
   MainLocal.CriticalInitDone = true;

   CHK (UtilInit    ())
   CHK (HashInit    ())
   CHK (QtUtilInit  ())
   CHK (t_File::Init())
   CHK (AaffInit    ())
   CHK (AewfInit    ())

   switch (CONFIG(NumberStyle))
   {
      case CFG_NUMBERSTYLE_DECIMAL_COMMA: MainLocal.pNumberLocale = new QLocale("de_DE"); break;
      case CFG_NUMBERSTYLE_DECIMAL_POINT: MainLocal.pNumberLocale = new QLocale("C"    ); break;
      case CFG_NUMBERSTYLE_LOCALE       : MainLocal.pNumberLocale = new QLocale(       ); break;
      default: CHK_CONST (ERROR_MAIN_INVALID_NUMBERSTYLE)
   }

   // Initialise libewf
   // -----------------
   
   #if (ENABLE_LIBEWF)
      #if (LIBEWF_VERSION >= 20130416)
         libewf_notify_set_stream  (stderr, nullptr);
         libewf_notify_set_verbose (CONFIG (VerboseLibewf) ? 1 : 0);
      #else
         libewf_set_notify_values (stderr, CONFIG (VerboseLibewf) ? 1 : 0);
      #endif
   #endif

   // Create central data structures
   // ------------------------------
   ShowSplashText (pSplashScreen, "Creating data structures");
   MainLocal.pDeviceList = new t_DeviceList;

   // Create main window, remove splashscreen and enter main loop
   // -----------------------------------------------------------

   MainLocal.pMainWindow = new t_MainWindow (MainLocal.pDeviceList);
   CHK_QT_EXIT (MainLocal.pApp->connect(MainLocal.pMainWindow, SIGNAL(SignalAutoExit(void)), MainLocal.pApp, SLOT(quit(void))))

   CHK (MainWindowAdjustSize (MainLocal.pMainWindow))

   if (pSplashScreen)
   {
      pSplashScreen->finish (MainLocal.pMainWindow);
      delete pSplashScreen;
   }

   RetCode = QApplication::exec();
   CHK (RetCode)
   CHK (FifoGetStatistics (FifoAllocs, FifoFrees, FifoAllocated))
   FifoRemaining = FifoAllocs - FifoFrees;
   LOG_INFO ("FIFO buffer statistics: %llu allocated - %llu freed = %lld remaining (%lld bytes)", FifoAllocs, FifoFrees, FifoRemaining, FifoAllocated)

   *pAutoExit = MainLocal.pMainWindow->AutoExit();

   // Deinitialise and exit the program
   // ---------------------------------
   delete MainLocal.pMainWindow;
   delete MainLocal.pDeviceList;
   delete MainLocal.pNumberLocale;
   MainLocal.pMainWindow   = nullptr;
   MainLocal.pDeviceList   = nullptr;
   MainLocal.pNumberLocale = nullptr;

   CHK (AewfDeInit  ())
   CHK (AaffDeInit  ())
   CHK (QtUtilDeInit())
   CHK (HashDeInit  ())
   CHK (UtilDeInit  ())
   CHK (CfgDeInit   ())

   delete MainLocal.pApp;

   if (CONFIG (SignalHandling))
      CHK (ToolSignalDeInit ())

   CHK_EXIT (ErrorDeInit())

   LOG_INFO ("=================================================== GUYMAGER ended normally ===================================================")
   delete MainLocal.pLog;

   return NO_ERROR;
}

APIRET MainGetCommandLine (char **ppCommandLine)
{
   *ppCommandLine = MainLocal.pCommandLine;
   return NO_ERROR;
}

QLocale *MainGetpNumberLocale (void)
{
   return MainLocal.pNumberLocale;
}

int main (int argc, char *argv[])
{
   APIRET        rc;
   int           i;
   size_t        Len=0;
   int           ExitCode = EXIT_NO_ERROR;
   bool          AutoExit;
   const char  *pFilenameMallocTracing;

   pFilenameMallocTracing = getenv (MAIN_ENVVAR_MALLOC_TRACE);
   if (pFilenameMallocTracing != nullptr)  // Switch on memory allocation tracing,
   {
      printf ("\nSwitching malloc tracing on, output file is %s", pFilenameMallocTracing);
      mtrace();                                    // see tracing functionality in GNU C lib
   }
   setbuf(stdout, nullptr);
   setbuf(stderr, nullptr);
   setlocale (LC_ALL, "");

   memset (&MainLocal, 0, sizeof(MainLocal));

   // Copy the whole command line (for documentation purposes)
   // --------------------------------------------------------
   for (i=0; i<argc; i++)
      Len += strlen (argv[i])+1;

   MainLocal.pCommandLine = (char *) malloc (Len);
   MainLocal.pCommandLine[0] = '\0';

   for (i=0; i<argc; i++)
   {
      if (i)
         strcat (MainLocal.pCommandLine, " ");
      strcat (MainLocal.pCommandLine, argv[i]);
   }

   rc = MainGo (argc, argv, &AutoExit);
   if (AutoExit)
      ExitCode = EXIT_AUTOEXIT;

   free (MainLocal.pCommandLine);

   printf ("\n");
   if ((rc == TOOLCFG_ERROR_CONFIG_ERROR) ||
       (rc == ERROR_CFG_INCOHERENCY_DETECTED))
        printf ("Error in configuration file. See log file for details.\n");
   else CHK_EXIT (rc)

   return ExitCode;
}

t_pDeviceList MainGetDeviceList (void)
{
   return MainLocal.pDeviceList;
}

