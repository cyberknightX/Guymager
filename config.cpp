// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Application configuration data
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

#include <limits.h>
#include <unistd.h>



#include <qcolor.h>
#include <qfont.h>

#include "toolcfg.h"
#include "toolsysinfo.h"
#include "file.h"

#include "ewf.h"

#include "qtutil.h"
#include "util.h"
#include "config.h"

// ------------------------------------
//              Constants
// ------------------------------------

static const char * CMDLINE_OPTION_LOG = "LOG";
static const char * CMDLINE_OPTION_CFG = "CFG";

static const char * DEFAULT_LOG_FILENAME  = "/var/log/guymager.log";
static const char * DEFAULT_CFG_FILENAME  = "/etc/guymager/guymager.cfg";
static const char * TEMPLATE_CFG_FILENAME = "template.cfg";

static const char * CFG_SECTION_GUYMAGER = "GUYMAGER";

static const unsigned char CONFIG_DUMMY_FILL = 0xAA;

static const uint CFG_MAX_COMPRESSIONTHREADS = 16;

static const int  CFG_MAX_LIMIT_JOBS = 4;

// ------------------------------------
//          Type definitions
// ------------------------------------


typedef struct
{
   t_CfgData                   CfgData;
   t_CfgBuffFont               CfgBuffFont;
   t_CfgColumn                 CfgBuffColumn;
   t_CfgBuffColor              CfgBuffColor;
   t_CfgBuffLocalHiddenDevice  CfgBuffLocalHiddenDevice;
   t_CfgBuffDeviceInfoCommand  CfgBuffDeviceInfoCommand;
   t_CfgBuffDlgAcquireField    CfgBuffDlgAcquireField;
   t_CfgBuffDlgAcquireRule     CfgBuffDlgAcquireRule;
   QFont  *                    FontArr [FONTOBJECT_COUNT];
   QColor *                    ColorArr[COLOR_COUNT];
   t_CfgColumn                 ColumnArr[CFG_MAX_COLUMNS];
   int                         Columns;
   QStringList                 LocalDevices;
   QStringList                 HiddenDevices;
   QStringList               *pCurrentDeviceList;
   QStringList                 DeviceInfoCommands;
   t_CfgDlgAcquireFields       DlgAcquireFields;
   t_CfgDlgAcquireRules        DlgAcquireRules;
   QStringList                 DlgAcquireFieldNames;

   t_ToolSysInfoMacAddr        MacAddr;
   QString                     HostName;
} t_CfgLocal;

// ------------------------------------
//          Global variables
// ------------------------------------

static t_CfgLocal CfgLocal;


// ------------------------------------
//             Prototypes
// ------------------------------------

bool MainWindowColumnExists (const char *pName);

// Start and SaveAndNext prototypes are declared here, as the declaration in the proper header files
// would require too manu include files for CONFIG.CPP and too much unnecessary recompilation.

static APIRET CfgFontStart        (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgFontSaveAndNext                    (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgFontEnd                                             (t_pcchar *ppErrorText);

static APIRET CfgColumnStart      (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgColumnSaveAndNext                  (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgColumnEnd                                           (t_pcchar *ppErrorText);

static APIRET CfgColorStart       (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgColorSaveAndNext                   (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgColorEnd                                            (t_pcchar *ppErrorText);

static APIRET CfgLocalDeviceStart       (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgHiddenDeviceStart      (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgLocalHiddenDeviceSaveAndNext             (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgLocalHiddenDeviceEnd                                      (t_pcchar *ppErrorText);

static APIRET CfgDeviceInfoCommandStart       (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgDeviceInfoCommandSaveAndNext                   (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgDeviceInfoCommandEnd                                            (t_pcchar *ppErrorText);

static APIRET CfgDlgAcquireFieldStart       (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgDlgAcquireFieldSaveAndNext                   (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgDlgAcquireFieldEnd                                            (t_pcchar *ppErrorText);

static APIRET CfgDlgAcquireRuleStart       (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgDlgAcquireRuleSaveAndNext                   (long *pBaseAddr, t_pcchar *ppErrorText);
static APIRET CfgDlgAcquireRuleEnd                                            (t_pcchar *ppErrorText);

static APIRET CfgIniLang (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText);
static APIRET CfgIniCPUs (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText);
static APIRET CfgIniMem  (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText);
static APIRET CfgIniJobs (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText);

// ------------------------------------
//     Configuration parameter table
// ------------------------------------

static t_ToolCfgSet SetArrBoolean[] =
{
   // Name in cfg file  Corresponding value
   // --------------------------------------
   {  "YES"           , true          },
   {  "ON"            , true          },
   {  "TRUE"          , true          },
   {  "1"             , true          },
   {  "ENABLED"       , true          },
   {  "ACTIVATED"     , true          },
   {  "NO"            , false         },
   {  "OFF"           , false         },
   {  "FALSE"         , false         },
   {  "0"             , false         },
   {  "DISABLED"      , false         },
   {  "DEACTIVATED"   , false         },
   {   nullptr        , 0             }
};

static t_ToolCfgSet SetArrBooleanAuto[] =
{
   // Name in cfg file  Corresponding value
   // --------------------------------------
   {  "YES"           , CFG_BOOLEANAUTO_TRUE },
   {  "ON"            , CFG_BOOLEANAUTO_TRUE },
   {  "TRUE"          , CFG_BOOLEANAUTO_TRUE },
   {  "1"             , CFG_BOOLEANAUTO_TRUE },
   {  "ENABLED"       , CFG_BOOLEANAUTO_TRUE },
   {  "ACTIVATED"     , CFG_BOOLEANAUTO_TRUE },
   {  "NO"            , CFG_BOOLEANAUTO_FALSE},
   {  "OFF"           , CFG_BOOLEANAUTO_FALSE},
   {  "FALSE"         , CFG_BOOLEANAUTO_FALSE},
   {  "0"             , CFG_BOOLEANAUTO_FALSE},
   {  "DISABLED"      , CFG_BOOLEANAUTO_FALSE},
   {  "DEACTIVATED"   , CFG_BOOLEANAUTO_FALSE},
   {  "AUTO"          , CFG_BOOLEANAUTO_AUTO },
   {   nullptr        , 0             }
};

static t_ToolCfgSet SetArrStartupSize[] =
{
   // Name in cfg file  Corresponding value
   // -------------------------------------------
   {  "STANDARD"      , CFG_STARTUPSIZE_STANDARD  },
   {  "FULLSCREEN"    , CFG_STARTUPSIZE_FULLSCREEN},
   {  "MAXIMIZED"     , CFG_STARTUPSIZE_MAXIMIZED },
   {  "MAXIMISED"     , CFG_STARTUPSIZE_MAXIMIZED },
   {  "MANUAL"        , CFG_STARTUPSIZE_MANUAL    },
   {   nullptr        , 0                         }
};

static t_ToolCfgSet SetArrNumberStyle[] =
{
   // Name in cfg file  Corresponding value
   // -------------------------------------------
   {  "LOCALE"        , CFG_NUMBERSTYLE_LOCALE       },
   {  "DECIMALCOMMA"  , CFG_NUMBERSTYLE_DECIMAL_COMMA},
   {  "DECIMALPOINT"  , CFG_NUMBERSTYLE_DECIMAL_POINT},
   {   nullptr        , 0                            }
};

static t_ToolCfgSet SetArrEntryMode[] =
{
   // Name in cfg file  Corresponding value
   // -------------------------------------------
   {  "HIDE"          , CFG_ENTRYMODE_HIDE       },
   {  "SHOWDEFAULT"   , CFG_ENTRYMODE_SHOWDEFAULT},
   {  "SHOWLAST"      , CFG_ENTRYMODE_SHOWLAST   },
   {   nullptr        , 0                        }
};

static t_ToolCfgSet SetArrFontObject[] =
{
   // Name in cfg file      Corresponding value
   // ------------------------------------------------------
   {  "Menu"              , FONTOBJECT_MENU               },
   {  "Toolbar"           , FONTOBJECT_TOOLBAR            },
   {  "Table"             , FONTOBJECT_TABLE              },
   {  "InfoField"         , FONTOBJECT_INFOFIELD          },
   {  "AcquisitionDialogs", FONTOBJECT_ACQUISITION_DIALOGS},
   {  "MessageDialogs"    , FONTOBJECT_MESSAGE_DIALOGS    },
   {  "DialogData"        , FONTOBJECT_DIALOG_DATA        },
   {   nullptr            , 0                             }
};

static t_ToolCfgSet SetArrAlignment[] =
{
   // Name in cfg file   Corresponding value
   // ---------------------------------------
   {  "LEFT"           , Qt::AlignLeft   },
   {  "RIGHT"          , Qt::AlignRight  },
   {  "CENTER"         , Qt::AlignCenter }
};

static t_ToolCfgSet SetArrColor[] =
{
   // Name in cfg file                Corresponding value
   // ---------------------------------------------------------------
   {  "LocalDevices"                , COLOR_LOCALDEVICES                   },
   {  "AdditionalState1"            , COLOR_ADDITIONALSTATE1               },
   {  "AdditionalState2"            , COLOR_ADDITIONALSTATE2               },
   {  "AdditionalState3"            , COLOR_ADDITIONALSTATE3               },
   {  "AdditionalState4"            , COLOR_ADDITIONALSTATE4               },
   {  "StateIdle"                   , COLOR_STATE_IDLE                     },
   {  "StateQueued"                 , COLOR_STATE_QUEUED                   },
   {  "StateAcquire"                , COLOR_STATE_ACQUIRE                  },
   {  "StateAcquirePaused"          , COLOR_STATE_ACQUIRE_PAUSED           },
   {  "StateVerify"                 , COLOR_STATE_VERIFY                   },
   {  "StateVerifyPaused"           , COLOR_STATE_VERIFY_PAUSED            },
   {  "StateCleanup"                , COLOR_STATE_CLEANUP                  },
   {  "StateFinished"               , COLOR_STATE_FINISHED                 },
   {  "StateFinishedBadVerify"      , COLOR_STATE_FINISHED_BADVERIFY       },
   {  "StateFinishedDuplicateFailed", COLOR_STATE_FINISHED_DUPLICATE_FAILED},
   {  "StateAbortedUser"            , COLOR_STATE_ABORTED_USER             },
   {  "StateAbortedOther"           , COLOR_STATE_ABORTED_OTHER            },
   {   nullptr                      , 0                                    }
};


static t_ToolCfgSet SetArrFormat[] =
{
   // Name in cfg file   Corresponding value
   // ------------------------------------------
   {  "DD"      ,        t_File::DD  },
   {  "EWF"     ,        t_File::EWF },
   {  "AFF"     ,        t_File::AAFF},
   {  "AAFF"    ,        t_File::AAFF},
   {   nullptr  ,        0           }
};

static t_ToolCfgSet SetArrEwfFormat[] =
{
   // Name in cfg file   Corresponding value
   // ------------------------------------------
   #if (ENABLE_LIBEWF)
      {  "Encase1" ,         LIBEWF_FORMAT_ENCASE1},
      {  "Encase2" ,         LIBEWF_FORMAT_ENCASE2},
      {  "Encase3" ,         LIBEWF_FORMAT_ENCASE3},
      {  "Encase4" ,         LIBEWF_FORMAT_ENCASE4},
      {  "Encase5" ,         LIBEWF_FORMAT_ENCASE5},
      {  "Encase6" ,         LIBEWF_FORMAT_ENCASE6},
      {  "Smart"   ,         LIBEWF_FORMAT_SMART  },
      {  "FTK"     ,         LIBEWF_FORMAT_FTK    },
      {  "Linen5"  ,         LIBEWF_FORMAT_LINEN5 },
      {  "Linen6"  ,         LIBEWF_FORMAT_LINEN6 },
      #if (LIBEWF_VERSION >= 20130416)
      {  "Encase7" ,         LIBEWF_FORMAT_ENCASE7},
      {  "Linen7"  ,         LIBEWF_FORMAT_LINEN7 },
      {  "EWFX"    ,         LIBEWF_FORMAT_EWFX   },
      #endif
   #endif
   {     "Guymager",         t_File::AEWF         }, // AEWF format is a format on its own (for the function calls) and at the same time a
   {     "AEWF"    ,         t_File::AEWF         }, // subformat of EWF (for the user interface). Looks a bit strange at first sight, but
   {      nullptr  ,         0                    }  // probably is the best compromise.
};


static t_ToolCfgSet SetArrEwfCompression[] =
{
   // Name in cfg file  Corresponding value
   // ----------------------------------------
   {  "None",           LIBEWF_COMPRESSION_NONE },
   {  "Empty",          LIBEWF_COMPRESSION_EMPTY},
   {  "Fast",           LIBEWF_COMPRESSION_FAST },
   {  "Best",           LIBEWF_COMPRESSION_BEST },
   {   nullptr ,        0                       }
};

#define CONFIG_COMPRESSIONTHREADS_AUTO -1
static t_ToolCfgSet SetArrCompressionThreads[] =
{
   // Name in cfg file  Corresponding value
   // ----------------------------------------
   {  "Auto",           CONFIG_COMPRESSIONTHREADS_AUTO},
   {   "0",              0  },  // Do not remove, this value will force guymager to use the non-paralellised functions of libewf
   {   "1",              1  },
   {   "2",              2  },
   {   "3",              3  },
   {   "4",              4  },
   {   "5",              5  },
   {   "6",              6  },
   {   "7",              7  },
   {   "8",              8  },
   {   "9",              9  },
   {  "10",             10  },
   {  "11",             11  },
   {  "12",             12  },
   {  "13",             13  },
   {  "14",             14  },
   {  "15",             15  },
   {  "16",             16  },
   {   nullptr,          0  }
};

static t_ToolCfgSet SetArrMaximumJobs[] =
{
   // Name in cfg file  Corresponding value
   // ----------------------------------------
   {  "Off",            CONFIG_LIMITJOBS_OFF },
   {  "Auto",           CONFIG_LIMITJOBS_AUTO},
   {   "1",              1  },
   {   "2",              2  },
   {   "3",              3  },
   {   "4",              4  },
   {   "5",              5  },
   {   "6",              6  },
   {   "7",              7  },
   {   "8",              8  },
   {   "9",              9  },
   {  "10",             10  },
   {  "11",             11  },
   {  "12",             12  },
   {  "13",             13  },
   {  "14",             14  },
   {  "15",             15  },
   {  "16",             16  },
   {   nullptr,          0  }
};


static t_ToolCfgSet SetArrDeviceScanMethod[] =
{
   // Name in cfg file  Corresponding value
   // ----------------------------------------
   { "libparted",       SCANMETHOD_LIBPARTED },
   { "parted",          SCANMETHOD_LIBPARTED },
   { "DBusHAL",         SCANMETHOD_DBUSHAL   },
   { "HAL",             SCANMETHOD_DBUSHAL   },
   { "DBusDevKit",      SCANMETHOD_DBUSDEVKIT},
   { "DevKit",          SCANMETHOD_DBUSDEVKIT},
   { "DBusUDisks",      SCANMETHOD_DBUSDEVKIT},
   { "UDisks",          SCANMETHOD_DBUSDEVKIT},
   { "libudev",         SCANMETHOD_LIBUDEV   },
   { "udev",            SCANMETHOD_LIBUDEV   },
   {  nullptr,          0                    }
};

static t_ToolCfgSet SetArrEwfNaming[] =
{
   // Name in cfg file  Corresponding value
   // ----------------------------------------
   { "Old",             EWFNAMING_OLD },
   { "FTK",             EWFNAMING_FTK },
   {  nullptr,          0             }
};


#define ELT(Elt)      ((long) &CfgLocal.CfgData.Elt)
#define ELT_SIZ(Elt)  ELT(Elt), (sizeof (CfgLocal.CfgData.Elt)-1)                      // for strings only, thus substract 1 byte (for terminating 0)
#define INIARR(Arr)   ((t_pToolCfgSet) (t_pvoid)&Arr)


//lint -e545     Suspicious use of &

static t_ToolCfgParamDesc CfgParamDescArr[] =
{
   // Assignment             CallOn       ParameterName                      Type             DestinationAddress                                Len      Min      Max  SetArray
   // see t_CfgAssignment    InitFn       in cfg file                        see t_CfgType    type is t_uint                                                           (CFGTYPE_SET only)
   // --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
   {CFGASSIGN_CMD          , nullptr    , {CMDLINE_OPTION_LOG              , CFGTYPE_NULL   , 0                                  ,                0,       0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_CMD          , nullptr    , {CMDLINE_OPTION_CFG              , CFGTYPE_NULL   , 0                                  ,                0,       0,       0, nullptr                         }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, CfgIniLang , {"Language"                      , CFGTYPE_STRING , ELT_SIZ(Language)                  ,                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"CheckRootRights"               , CFGTYPE_SET    , ELT_SIZ(CheckRootRights)           ,                         0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"StartupSize"                   , CFGTYPE_SET    , ELT(StartupSize)                   ,                0,       0,       0, INIARR(SetArrStartupSize)       }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"StartupSizeManualX"            , CFGTYPE_INTEGER, ELT(StartupSizeManualX            ),                0,       0,    3000, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"StartupSizeManualY"            , CFGTYPE_INTEGER, ELT(StartupSizeManualY            ),                0,       0,    3000, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"StartupSizeManualDx"           , CFGTYPE_INTEGER, ELT(StartupSizeManualDx           ),                0,       0,    3000, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"StartupSizeManualDy"           , CFGTYPE_INTEGER, ELT(StartupSizeManualDy           ),                0,       0,    3000, nullptr                         }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FileDialogSize"                , CFGTYPE_SET    , ELT(FileDialogSize)                ,                0,       0,       0, INIARR(SetArrStartupSize)       }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FileDialogSizeManualDx"        , CFGTYPE_INTEGER, ELT(FileDialogSizeManualDx        ),                0,       0,    3000, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FileDialogSizeManualDy"        , CFGTYPE_INTEGER, ELT(FileDialogSizeManualDy        ),                0,       0,    3000, nullptr                         }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"NumberStyle"                   , CFGTYPE_SET    , ELT(NumberStyle)                   ,                0,       0,       0, INIARR(SetArrNumberStyle)       }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"ScreenRefreshInterval"         , CFGTYPE_INTEGER, ELT(ScreenRefreshInterval         ),                0,       1, 8640000, nullptr                          }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"UseFileDialogFromQt"           , CFGTYPE_SET    , ELT(UseFileDialogFromQt)           ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"UserFieldName"                 , CFGTYPE_STRING , ELT_SIZ(UserFieldName             ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AdditionalStateInfoName"       , CFGTYPE_STRING , ELT_SIZ(AdditionalStateInfoName   ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"WarnAboutImageSize"            , CFGTYPE_SET    , ELT(WarnAboutImageSize)            ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"WarnAboutSegmentFileCount"     , CFGTYPE_SET    , ELT(WarnAboutSegmentFileCount)     ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"DeleteAbortedImageFiles"       , CFGTYPE_SET    , ELT(DeleteAbortedImageFiles)       ,                0,       0,       0, INIARR(SetArrBooleanAuto)       }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AutoExit"                      , CFGTYPE_SET    , ELT(AutoExit)                      ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AutoExitCountdown"             , CFGTYPE_INTEGER, ELT(AutoExitCountdown)             ,                0,       3,  864000, nullptr                         }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"DefaultFormat"                 , CFGTYPE_SET    , ELT(DefaultFormat)                 ,                0,       0,       0, INIARR(SetArrFormat)            }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"InfoFieldsForDd"               , CFGTYPE_SET    , ELT(InfoFieldsForDd)               ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"EwfFormat"                     , CFGTYPE_SET    , ELT(EwfFormat     )                ,                0,       0,       0, INIARR(SetArrEwfFormat)         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"EwfCompression"                , CFGTYPE_SET    , ELT(EwfCompression)                ,                0,       0,       0, INIARR(SetArrEwfCompression)    }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"EwfCompressionThreshold"       , CFGTYPE_DOUBLE , ELT(EwfCompressionThreshold)       ,                0,     0.0,     1.0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"EwfNaming"                     , CFGTYPE_SET    , ELT(EwfNaming)                     ,                0,       0,       0, INIARR(SetArrEwfNaming)         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AffEnabled"                    , CFGTYPE_SET    , ELT(AffEnabled)                    ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AffCompression"                , CFGTYPE_INTEGER, ELT(AffCompression)                ,                0,       1,       9, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AffMarkBadSectors"             , CFGTYPE_SET    , ELT(AffMarkBadSectors)             ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"SpecialFilenameChars"          , CFGTYPE_STRING , ELT_SIZ(SpecialFilenameChars)      ,                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"CalcImageFileMD5"              , CFGTYPE_SET    , ELT(CalcImageFileMD5)              ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"DuplicateImage"                , CFGTYPE_SET    , ELT(DuplicateImage)                ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"DirectoryFieldEditing"         , CFGTYPE_SET    , ELT(DirectoryFieldEditing)         ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AllowPathInFilename"           , CFGTYPE_SET    , ELT(AllowPathInFilename)           ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"ConfirmDirectoryCreation"      , CFGTYPE_SET    , ELT(ConfirmDirectoryCreation)      ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AvoidEncaseProblems"           , CFGTYPE_SET    , ELT(AvoidEncaseProblems)           ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"AvoidCifsProblems"             , CFGTYPE_SET    , ELT(AvoidCifsProblems)             ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"DeviceScanMethod"              , CFGTYPE_SET    , ELT(DeviceScanMethod)              ,                0,       0,       0, INIARR(SetArrDeviceScanMethod)  }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"ScanInterval"                  , CFGTYPE_INTEGER, ELT(ScanInterval                  ),                0,       1, 8640000, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"CommandGetSerialNumber"        , CFGTYPE_STRING , ELT_SIZ(CommandGetSerialNumber    ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"ForceCommandGetSerialNumber"   , CFGTYPE_SET    , ELT(ForceCommandGetSerialNumber   ),                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"CommandGetAddStateInfo"        , CFGTYPE_STRING , ELT_SIZ(CommandGetAddStateInfo    ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"CommandAcquisitionEnd"         , CFGTYPE_STRING , ELT_SIZ(CommandAcquisitionEnd     ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"QueryDeviceMediaInfo"          , CFGTYPE_SET    , ELT(QueryDeviceMediaInfo          ),                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"DirectIO"                      , CFGTYPE_SET    , ELT(DirectIO                      ),                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"RunStatsTemplateActive"        , CFGTYPE_STRING , ELT_SIZ(RunStatsTemplateActive    ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"RunStatsTemplateEnded"         , CFGTYPE_STRING , ELT_SIZ(RunStatsTemplateEnded     ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"RunStatsOutput"                , CFGTYPE_STRING , ELT_SIZ(RunStatsOutput            ),                         0,       0, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"RunStatsInterval"              , CFGTYPE_INTEGER, ELT(RunStatsInterval              ),                0,       1,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FifoBlockSizeDD"               , CFGTYPE_INTEGER, ELT(FifoBlockSizeDD               ),                0,       0,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FifoBlockSizeEWF"              , CFGTYPE_INTEGER, ELT(FifoBlockSizeEWF              ),                0,       0,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FifoBlockSizeAFF"              , CFGTYPE_INTEGER, ELT(FifoBlockSizeAFF              ),                0,       0,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, CfgIniMem  , {"FifoMaxMem"                    , CFGTYPE_INTEGER, ELT(FifoMaxMem                    ),                0,       0,    2000, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"FifoMemoryManager"             , CFGTYPE_SET    , ELT(FifoMemoryManager)             ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"UseSeparateHashThread"         , CFGTYPE_SET    , ELT(UseSeparateHashThread)         ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, CfgIniCPUs , {"CompressionThreads"            , CFGTYPE_SET    , ELT(CompressionThreads)            ,                0,       0,    1024, INIARR(SetArrCompressionThreads)}, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"BadSectorLogThreshold"         , CFGTYPE_INTEGER, ELT(BadSectorLogThreshold)         ,                0,       0,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"BadSectorLogModulo"            , CFGTYPE_INTEGER, ELT(BadSectorLogModulo)            ,                0,       1,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, CfgIniJobs , {"LimitJobs"                     , CFGTYPE_SET    , ELT(LimitJobs)                     ,                0,       1,       8, INIARR(SetArrMaximumJobs)       }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"JobMaxBadSectors"              , CFGTYPE_INTEGER, ELT(JobMaxBadSectors)              ,                0,       0,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"JobDisconnectTimeout"          , CFGTYPE_INTEGER, ELT(JobDisconnectTimeout)          ,                0,       0,99999999, nullptr                         }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"LocalHiddenDevicesUseRegExp"   , CFGTYPE_SET    , ELT(LocalHiddenDevicesUseRegExp)   ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"SignalHandling"                , CFGTYPE_SET    , ELT(SignalHandling)                ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"WriteToDevNull"                , CFGTYPE_SET    , ELT(WriteToDevNull)                ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"UseMemWatch"                   , CFGTYPE_SET    , ELT(UseMemWatch)                   ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"VerboseLibewf"                 , CFGTYPE_SET    , ELT(VerboseLibewf)                 ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},
   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {"CheckEwfData"                  , CFGTYPE_SET    , ELT(CheckEwfData)                  ,                0,       0,       0, INIARR(SetArrBoolean)           }, CFG_FILLUP_FORLINT},

   {CFGASSIGN_BOTH_MULTIPLE, nullptr    , {nullptr                         ,(t_ToolCfgType)0,                                   0,                0,       0,       0, nullptr                         }, CFG_FILLUP_FORLINT}
};


// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: Fonts
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgBuffFont pFont0 = nullptr;
#undef  ELT
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pFont0->Elt)
#define ELT_SIZ(Elt)  (long)&pFont0->Elt, sizeof (pFont0->Elt)-1  // for strings only, thus substract 1 byte (for terminating 0)

static t_ToolCfgDataDesc CfgDescArrFont[] =
{
   // ParameterName Type (see       DestinationOfs   Len  Min  Max  SetArray
   // in cfg file   t_ToolCfgType)  type is long                    (CFGTYPE_SET only)
   // --------------------------------------------------------------------------------
   {  "Object",     CFGTYPE_SET    , ELT(Object)  ,    0,   0,   0, SetArrFontObject},
   {  "Family",     CFGTYPE_STRING , ELT_SIZ(Family)    ,   0,   0, nullptr         },
   {  "Size",       CFGTYPE_INTEGER, ELT(Size)    ,    0,   0, 100, nullptr         },
   {  "Weight",     CFGTYPE_INTEGER, ELT(Weight)  ,    0,   0, 100, nullptr         },
   {  "Italic",     CFGTYPE_SET    , ELT(Italic)  ,    0,   0,   0, SetArrBoolean   },
   {  nullptr,      CFGTYPE_NULL   , 0            ,    0,   0,   0, nullptr         }
};

// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: Columns
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgColumn pColumn0 = nullptr;
#undef  ELT
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pColumn0->Elt)
#define ELT_SIZ(Elt)  (long)&pColumn0->Elt, sizeof (pColumn0->Elt)-1  // for strings only, thus substract 1 byte (for terminating 0)

static t_ToolCfgDataDesc CfgDescArrColumn[] =
{
   // ParameterName      Type (see        DestinationOfs          Len  Min   Max  SetArray
   // in cfg file        t_ToolCfgType)   type is long                            (CFGTYPE_SET only)
   // ----------------------------------------------------------------------------------------------
   {  "ColumnName"     , CFGTYPE_STRING , ELT_SIZ(Name)              ,   0,    0, nullptr         },
   {  "Alignment"      , CFGTYPE_SET    , ELT(Alignment)       ,    0,   0,    0, SetArrAlignment },
   {  "MinWidth"       , CFGTYPE_INTEGER, ELT(MinWidth)        ,    0,   0, 9999, nullptr         },
   {  "ShowInMainTable", CFGTYPE_SET    , ELT(ShowInMainTable) ,    0,   0,    0, SetArrBoolean   },
   {  "ShowInCloneTabe", CFGTYPE_SET    , ELT(ShowInCloneTable),    0,   0,    0, SetArrBoolean   },
   {  nullptr          , CFGTYPE_NULL   , 0                    ,    0,   0,    0, nullptr         }
};

// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: Colors
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgBuffColor pColor0 = nullptr;
#undef  ELT
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pColor0->Elt)

static t_ToolCfgDataDesc CfgDescArrColor[] =
{
   // ParameterName Type (see       DestinationOfs   Len  Min  Max  SetArray
   // in cfg file   t_ToolCfgType)  type is long                    (CFGTYPE_SET only)
   // --------------------------------------------------------------------------------
   {  "Color",      CFGTYPE_SET    , ELT(Color)   ,    0,   0,   0, SetArrColor     },
   {  "R",          CFGTYPE_INTEGER, ELT(R)       ,    0,   0, 255, nullptr         },
   {  "G",          CFGTYPE_INTEGER, ELT(G)       ,    0,   0, 255, nullptr         },
   {  "B",          CFGTYPE_INTEGER, ELT(B)       ,    0,   0, 255, nullptr         },
   {  nullptr,      CFGTYPE_NULL   , 0            ,    0,   0,   0, nullptr         }
};

// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: LocalHiddenDevices
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgBuffLocalHiddenDevice pLoc0 = nullptr;
#undef  ELT
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pLoc0->Elt)
#define ELT_SIZ(Elt)  (long)&pLoc0->Elt, sizeof (pLoc0->Elt)-1  // for strings only, thus substract 1 byte (for terminating 0)

static t_ToolCfgDataDesc CfgDescArrLocalHiddenDevice[] =
{
   // ParameterName Type (see       DestinationOfs   Len  Min  Max  SetArray
   // in cfg file   t_ToolCfgType)  type is long                    (CFGTYPE_SET only)
   // --------------------------------------------------------------------------------
   {  "Device",     CFGTYPE_STRING , ELT_SIZ(Device)    ,   0,   0, nullptr         },
   {  nullptr,      CFGTYPE_NULL   , 0            ,    0,   0,   0, nullptr         }
};


// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: DeviceInfoCommands
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgBuffDeviceInfoCommand pCmd0 = nullptr;
#undef  ELT                               //lint !e750: local macro 'ELT' not referenced
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pCmd0->Elt)
#define ELT_SIZ(Elt)  (long)&pCmd0->Elt, sizeof (pCmd0->Elt)-1  // for strings only, thus substract 1 byte (for terminating 0)

static t_ToolCfgDataDesc CfgDescArrDeviceInfoCommand[] =
{
   // ParameterName Type (see       DestinationOfs   Len  Min  Max  SetArray
   // in cfg file   t_ToolCfgType)  type is long                    (CFGTYPE_SET only)
   // --------------------------------------------------------------------------------
   {  "Command",    CFGTYPE_STRING , ELT_SIZ(Command)   ,   0,   0, nullptr         },
   {  nullptr,      CFGTYPE_NULL   , 0            ,    0,   0,   0, nullptr         }
};

// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: DlgAcquireField
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgBuffDlgAcquireField pFld0 = nullptr;
#undef  ELT                               //lint !e750: local macro 'ELT' not referenced
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pFld0->Elt)
#define ELT_SIZ(Elt)  (long)&pFld0->Elt, sizeof (pFld0->Elt)-1  // for strings only, thus substract 1 byte (for terminating 0)

static t_ToolCfgDataDesc CfgDescArrDlgAcquireField[] =
{
   // ParameterName     Type (see        DestinationOfs     Len  Min  Max  SetArray
   // in cfg file       t_ToolCfgType)   type is long                      (CFGTYPE_SET only)
   // ---------------------------------------------------------------------------------------
   {  "FieldName"     , CFGTYPE_STRING , ELT_SIZ(FieldName)     ,  0,   0, nullptr        },
   {  "EntryModeImage", CFGTYPE_SET    , ELT(EntryModeImage),  0,  0,   0, SetArrEntryMode},
   {  "EntryModeClone", CFGTYPE_SET    , ELT(EntryModeClone),  0,  0,   0, SetArrEntryMode},
   {  "DefaultValue"  , CFGTYPE_STRING , ELT_SIZ(DefaultValue)  ,  0,   0, nullptr        },
   {  nullptr         , CFGTYPE_NULL   , 0                  ,  0,  0,   0, nullptr        }
};


// ----------------------------------------------------------------------------------------------------------------------
//                                               TABLE: DlgAcquireRule
// ----------------------------------------------------------------------------------------------------------------------

static t_pCfgBuffDlgAcquireRule pFil0 = nullptr;
#undef  ELT                               //lint !e750: local macro 'ELT' not referenced
#undef  ELT_SIZ
#define ELT(Elt)     ((long)&pFil0->Elt)
#define ELT_SIZ(Elt)  (long)&pFil0->Elt, sizeof (pFil0->Elt)-1  // for strings only, thus substract 1 byte (for terminating 0)

static t_ToolCfgDataDesc CfgDescArrDlgAcquireRule[] =
{
   // ParameterName           Type (see        DestinationOfs        Len  Min  Max  SetArray
   // in cfg file             t_ToolCfgType)   type is long                        (CFGTYPE_SET only)
   // -----------------------------------------------------------------------------------------------
   {  "TriggerFieldName"    , CFGTYPE_STRING , ELT_SIZ(TriggerFieldName),   0,   0, nullptr        },
   {  "DestinationFieldName", CFGTYPE_STRING , ELT_SIZ(DestFieldName)   ,   0,   0, nullptr        },
   {  "Value"               , CFGTYPE_STRING , ELT_SIZ(Value)           ,   0,   0, nullptr        },
   {  nullptr               , CFGTYPE_NULL   , 0                   ,   0,   0,   0, nullptr        }
};


// ----------------------------------------------------------------------------------------------------------------------
//                                                      Table descriptor
// ----------------------------------------------------------------------------------------------------------------------

#undef  ELT       //lint !e750: local macro 'ELT' not referenced
#undef  ELT_SIZ

static t_ToolCfgTableDesc CfgTableDescArr[] =
{
   // Tabletype            StartFn                        SaveAndNextFn                       EndFn                       pDataDescArray
   // -----------------------------------------------------------------------------------------------------------------------------------------------------
   {"Fonts"              , &CfgFontStart               , &CfgFontSaveAndNext               , CfgFontEnd               , &CfgDescArrFont              [0]},
   {"Columns"            , &CfgColumnStart             , &CfgColumnSaveAndNext             , CfgColumnEnd             , &CfgDescArrColumn            [0]},
   {"Colors"             , &CfgColorStart              , &CfgColorSaveAndNext              , CfgColorEnd              , &CfgDescArrColor             [0]},
   {"LocalDevices"       , &CfgLocalDeviceStart        , &CfgLocalHiddenDeviceSaveAndNext  , CfgLocalHiddenDeviceEnd  , &CfgDescArrLocalHiddenDevice [0]},
   {"HiddenDevices"      , &CfgHiddenDeviceStart       , &CfgLocalHiddenDeviceSaveAndNext  , CfgLocalHiddenDeviceEnd  , &CfgDescArrLocalHiddenDevice [0]},
   {"DeviceInfoCommands" , &CfgDeviceInfoCommandStart  , &CfgDeviceInfoCommandSaveAndNext  , CfgDeviceInfoCommandEnd  , &CfgDescArrDeviceInfoCommand [0]},
   {"DlgAcquireField"    , &CfgDlgAcquireFieldStart    , &CfgDlgAcquireFieldSaveAndNext    , CfgDlgAcquireFieldEnd    , &CfgDescArrDlgAcquireField   [0]},
   {"DlgAcquireRule"     , &CfgDlgAcquireRuleStart     , &CfgDlgAcquireRuleSaveAndNext     , CfgDlgAcquireRuleEnd     , &CfgDescArrDlgAcquireRule    [0]},
   {nullptr              , nullptr                     , nullptr                           , nullptr                  , nullptr                         }
};

//lint +e545     Suspicious use of &

// ------------------------------------
//              Functions
// ------------------------------------

t_pCfgData CfgGetpData (void)
{
   return &CfgLocal.CfgData;
}

APIRET CfgReadConfiguration (t_pcchar pCfgFileName)
{
   QString   DeskModel;
   QString   MacAddrSectionName;
   QString   HostSectionName;
   char      HostName[64];
   t_pcchar pMsg = nullptr;
   APIRET    rc;

   // Section name for MAC address (MACADDR_11BB33CC55DD)
   // ---------------------------------------------------
   rc = ToolSysInfoGetMacAddr (&CfgLocal.MacAddr);
   if (rc == TOOLSYSINFO_ERROR_NO_ADDR)
   {
      LOG_INFO ("MAC network hardware address: none found")
   }
   else
   {
      CHK (rc)
      LOG_INFO ("MAC network hardware address: %s", &CfgLocal.MacAddr.AddrStr[0])
      MacAddrSectionName  = "MACADDR_";
      MacAddrSectionName += &CfgLocal.MacAddr.AddrStr[0];
      CHK (ToolCfgAddGlobalSectionName (QSTR_TO_PSZ(MacAddrSectionName)))
   }

   // Section name for host name (HOST_thename)
   // ---------------------------------------------
   if (gethostname (&HostName[0], sizeof(HostName)))
   {
      LOG_INFO ("Error in gethostname.")
   }
   else
   {
      LOG_INFO ("Host name: %s", &HostName[0])
      CfgLocal.HostName = HostName;
      HostSectionName   = "HOST_";
      HostSectionName  += CfgLocal.HostName;
      CHK (ToolCfgAddGlobalSectionName (QSTR_TO_PSZ(HostSectionName)))
   }

   CHK (ToolCfgAddGlobalSectionName (CFG_SECTION_GUYMAGER))
   CHK (ToolCfgScanConfiguration (pCfgFileName, "", &CfgParamDescArr[0], &CfgTableDescArr[0]))
   CHK (ToolCfgLogConfiguration (CfgParamDescArr))

   // Configuration plausibility checks
   // ---------------------------------
   if (!CONFIG(AffEnabled) && (CONFIG(DefaultFormat) == t_File::AAFF))
      pMsg = "Problem in configuration detected: AffEnabled is off, but AFF is set as default format. Please correct settings for parameter AffEnabled or DefaultFormat.";
   if ((CONFIG (EwfFormat)      != t_File::AEWF) &&
       (CONFIG (EwfCompression) == LIBEWF_COMPRESSION_EMPTY))
      pMsg = "Parameter EwfCompression is set to 'Empty'; however, this setting only is allowed with EwfFormat set to 'Guymager'. Please correct settings for parameter EwfCompression or EwfFormat.";
   if (pMsg)
   {
      printf ("%s", pMsg);
      LOG_INFO ("%s", pMsg)
      CHK (ERROR_CFG_INCOHERENCY_DETECTED)
   }

   return NO_ERROR;
}

QString CfgGetHostName  (void)
{
   return CfgLocal.HostName;
}

QString CfgGetMacAddress (void)
{
   return QString(CfgLocal.MacAddr.AddrStr);
}


// ----------------------------------
//       CallOnInit functions
// ----------------------------------

APIRET CfgIniLang (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText)
{
   QString Language = CONFIG(Language);
   QString LanguageSystem;

   *ppErrorText = nullptr;
   if (Language.compare ("AUTO", Qt::CaseInsensitive) == 0)
   {
//      LanguageSystem = QLocale::system().name();       // This is not reliable. Qt seems to use LC_NUMERIC for selecting the language, which
      LanguageSystem = QString (getenv ("LANGUAGE"));    // is not what we want. So, let's read the LANGUAGE evironment variable ourselves.
      Language = LanguageSystem.split('_')[0];
      snprintf (CONFIG(Language), sizeof(CONFIG(Language)), "%s", QSTR_TO_PSZ(Language));

      LOG_INFO ("Parameter %s set to 'AUTO', switching to language '%s'", pCfgParamDesc->DataDesc.pName, CONFIG(Language))
   }
   return NO_ERROR;
}

APIRET CfgIniCPUs (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText)
{
   t_pcchar     pParamName;
   unsigned int  CPUs;

   *ppErrorText = nullptr;
   pParamName = pCfgParamDesc->DataDesc.pName;

   if (CONFIG (CompressionThreads) == CONFIG_COMPRESSIONTHREADS_AUTO)
   {
      CPUs = UtilGetNumberOfCPUs();
      LOG_INFO ("Parameter %s set to AUTO; %u CPUs detected", pParamName, CPUs)
      CONFIG (CompressionThreads) = (int)CPUs;
      if (CPUs > CFG_MAX_COMPRESSIONTHREADS)
      {
         CONFIG (CompressionThreads) = CFG_MAX_COMPRESSIONTHREADS;
         LOG_INFO ("Maximum value for %s is %d", pParamName, CFG_MAX_COMPRESSIONTHREADS)
      }
      LOG_INFO ("Setting %s to %d.", pParamName, CONFIG (CompressionThreads))
   }

   return NO_ERROR;
}

APIRET CfgIniJobs (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText)
{
   t_pcchar pParamName;
   int       CPUs;

   *ppErrorText = nullptr;
   pParamName = pCfgParamDesc->DataDesc.pName;

   if (CONFIG (LimitJobs) == CONFIG_LIMITJOBS_AUTO)
   {
      CPUs = (int) UtilGetNumberOfCPUs();
      LOG_INFO ("Parameter %s set to AUTO; %d processors detected", pParamName, CPUs)
      CONFIG (LimitJobs) = GETMIN (CPUs / 2, CFG_MAX_LIMIT_JOBS);
      LOG_INFO ("Setting %s to %d (half the number of CPUs, with a maximum of %d).", pParamName, CONFIG (LimitJobs), CFG_MAX_LIMIT_JOBS)
   }

   return NO_ERROR;
}

APIRET CfgIniMem (t_pToolCfgParamDesc pCfgParamDesc, t_pcchar *ppErrorText)
{
   t_pcchar           pParamName;
   unsigned long long  Bytes;

   *ppErrorText = nullptr;
   pParamName = pCfgParamDesc->DataDesc.pName;

//   meminfo();
//   Bytes = kb_main_total * 1024;

   Bytes = UtilGetInstalledRAM ();

   if (CONFIG (FifoMaxMem) == 0)
   {
      CONFIG (FifoMaxMem) = GETMAX (1, (int)(Bytes / (8*1024*1024)));   // Use one eighth of the available mem, convert to MB
      CONFIG (FifoMaxMem) = GETMIN (CONFIG (FifoMaxMem), 128);          // Stay below 128MB
      LOG_INFO ("Parameter %s set to 0 (auto); %0.1f MB of RAM detected.", pParamName, Bytes/(1024.0*1024.0));
      LOG_INFO ("Setting %s to %d (i.e. using %d MB per acquisition as FIFO memory)", pParamName, CONFIG (FifoMaxMem), CONFIG (FifoMaxMem))
   }

   return NO_ERROR;
}

// ----------------------------------
//  LOG and CFG command line options
// ----------------------------------

APIRET CfgGetLogFileName (t_pcchar *ppLogFileName, bool *pDefaultUsed)
{
   APIRET rc;
   bool   Def;

   Def = false;
   rc = ToolCfgGetCmdLineOption (CMDLINE_OPTION_LOG, ppLogFileName);
   if (rc == TOOLCFG_ERROR_CMDLINE_OPTION_NOT_FOUND)
   {
      *ppLogFileName = DEFAULT_LOG_FILENAME;
      Def = true;
   }
   else
   {
      CHK (rc)
   }
   if (pDefaultUsed)
      *pDefaultUsed = Def;

   return NO_ERROR;
}

APIRET CfgGetCfgFileName (t_pcchar *ppCfgFileName, bool *pDefaultUsed)
{
   APIRET    rc;
   bool      Def;
   t_pcchar pVersion;

   Def = false;
   rc = ToolCfgGetCmdLineOption (CMDLINE_OPTION_CFG, ppCfgFileName);  // Try to get the configuration file name
   if (rc == TOOLCFG_ERROR_CMDLINE_OPTION_NOT_FOUND)
   {
      *ppCfgFileName = DEFAULT_CFG_FILENAME;                          // Use default if not found
      Def = true;
   }
   else
   {
      CHK (rc)
      if(strcasecmp(*ppCfgFileName, "template") == 0)
      {
          pVersion = "Version unknown";
          CHK (ToolCfgBuildTemplate (TEMPLATE_CFG_FILENAME, pVersion, CFG_SECTION_GUYMAGER, &CfgParamDescArr[0], &CfgTableDescArr[0]))
          return ERROR_CFG_ONLY_TEMPLATE_GENERATED;
      }
   }
   if (pDefaultUsed)
      *pDefaultUsed = Def;

   return NO_ERROR;
}

// ------------------------------------
//                Fonts
// ------------------------------------

static APIRET CfgFontStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   memset (CfgLocal.FontArr, 0, sizeof(CfgLocal.FontArr));
   *pBaseAddr = (long) &CfgLocal.CfgBuffFont;
   *ppErrorText = nullptr;

//   #define CONFIG_SHOW_FONTS
   #ifdef CONFIG_SHOW_FONTS
      QFontDatabase FontDB;
      QString       Sizes;

      foreach (const QString &Family, FontDB.families())
      {
         foreach (const QString &Style, FontDB.styles(Family))
         {
            Sizes.clear ();
            foreach (int Size, FontDB.smoothSizes (Family, Style))
               Sizes += QString::number (Size) + " ";
            LOG_INFO ("Family/style: %s/%s  - Smooth sizes: %s", QSTR_TO_PSZ(Family), QSTR_TO_PSZ(Style), QSTR_TO_PSZ(Sizes.trimmed()))
         }
      }
   #endif

   return NO_ERROR;
}

static APIRET CfgFontSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   t_pCfgBuffFont pCfg;
   QFont         *pFont;
   QFont        **ppFontDest;
   const char    *pObjectName;

   pCfg = &CfgLocal.CfgBuffFont;

   if (strlen (pCfg->Family))
   {
      pFont = new QFont (pCfg->Family, pCfg->Size, pCfg->Weight, (bool)pCfg->Italic);
      ppFontDest = &CfgLocal.FontArr[pCfg->Object];
      if (*ppFontDest)            // This might happen if a fonts figures more
         delete *ppFontDest;      // than once in the configuration
      *ppFontDest = pFont;

      CHK (ToolCfgGetSetString (&SetArrFontObject[0], pCfg->Object, &pObjectName))

      LOG_INFO ("Font object %15s: Requested: %20s Size %2d Weight %2d %-8s",
                 pObjectName,
                 pCfg->Family, pCfg->Size, pCfg->Weight,
                 pCfg->Italic ? "Italic" : "NoItalic")

//      QFontInfo *pFontInfo = new QFontInfo (*pFont);
//      LOG_INFO ("                             Returned : %20s Size %2d Weight %2d %-8s PixelSize %d %s %s %s",
//                 pFontInfo->family    ().toAscii().constData(),
//                 pFontInfo->pointSize (),
//                 pFontInfo->weight    (),
//                 pFontInfo->italic    () ? "Italic" : "NoItalic",
//
//                 pFontInfo->pixelSize (),
//                 pFontInfo->fixedPitch() ? "FixedPitch" : "NoFixedPitch",
//                 pFontInfo->rawMode   () ? "RawMode"    : "NoRawMode"   ,
//                 pFontInfo->exactMatch() ? "ExactMatch" : "NoxactMatch" );
//      delete pFontInfo;
   }
   *pBaseAddr = (long) &CfgLocal.CfgBuffFont;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgFontEnd (t_pcchar *ppErrorText)
{
//   const char *pObjectName;
//   int          i;

   *ppErrorText = nullptr;
//   for (i=0; i<FONTOBJECT_COUNT; i++)
//   {
//      if (CfgLocal.FontArr[i] == nullptr)
//      {
//         CHK (ToolCfgGetSetString (&SetArrFontObject[0], i, &pObjectName))
//         LOG_INFO ("Font settings for object '%s' are missing", pObjectName)
//         *ppErrorText = "The table is incomplete, some fonts are missing";
//      }
//   }

   return NO_ERROR;
}

QFont *CfgGetpFont (t_CfgFontObject Object)
{
   if (Object < FONTOBJECT_COUNT)
        return (CfgLocal.FontArr[Object]);
   else return nullptr;
}

static APIRET CfgDestroyFonts (void)
{
   int i;

   for (i=0; i<FONTOBJECT_COUNT; i++)
   {
      if (CfgLocal.FontArr[i])
         delete CfgLocal.FontArr[i];
   }
   return NO_ERROR;
}

// ------------------------------------
//              Columns
// ------------------------------------

static APIRET CfgColumnStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CfgLocal.Columns = 0;

   *pBaseAddr = (long) &CfgLocal.CfgBuffColumn;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgColumnSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   *ppErrorText = nullptr;

   if      (CfgLocal.Columns >= CFG_MAX_COLUMNS)                     *ppErrorText = "Max. number of columns exceeded";
   else if (!MainWindowColumnExists (CfgLocal.CfgBuffColumn.Name))   *ppErrorText = "Non existent column name";
   else                                                              CfgLocal.ColumnArr[CfgLocal.Columns++] = CfgLocal.CfgBuffColumn;

   *pBaseAddr = (long) &CfgLocal.CfgBuffColumn;
   return NO_ERROR;
}

static APIRET CfgColumnEnd (t_pcchar *ppErrorText)
{
   *ppErrorText = nullptr;
   return NO_ERROR;
}

int CfgGetColumnCount (void)
{
   return CfgLocal.Columns;
}

t_pcCfgColumn CfgGetColumn (int i)
{
   return &CfgLocal.ColumnArr[i];
}

// ------------------------------------
//               Colors
// ------------------------------------

static APIRET CfgColorStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   int i;

   for (i=0; i<COLOR_COUNT; i++)
      CfgLocal.ColorArr[i] = nullptr;

   *pBaseAddr = (long) &CfgLocal.CfgBuffColor;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgColorSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   t_pCfgBuffColor pCfg;
   QColor         *pColor;

   pCfg = &CfgLocal.CfgBuffColor;
   pColor = new QColor (pCfg->R, pCfg->G, pCfg->B);
   CfgLocal.ColorArr[pCfg->Color] = pColor;

   *pBaseAddr = (long) &CfgLocal.CfgBuffColor;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgColorEnd (t_pcchar *ppErrorText)
{
   const char *pName;
   int          i;

   *ppErrorText = nullptr;
   for (i=0; i<COLOR_COUNT; i++)
   {
      if (CfgLocal.ColorArr[i] == nullptr)
      {
         CHK (ToolCfgGetSetString (&SetArrColor[0], i, &pName))
         LOG_INFO ("Color settings for object '%s' are missing", pName)
         *ppErrorText = "The table is incomplete, some colors are missing";
      }
   }

   return NO_ERROR;
}

const QColor *CfgGetpColor (t_CfgColor Color)
{
   if (Color == COLOR_DEFAULT)
   {
      return &QtUtilColorDefault;
   }
   else
   {
      if ((Color >= 0) && (Color < COLOR_COUNT))          //lint !e568: non-negative quantity is never less than zero
           return (CfgLocal.ColorArr[Color]);
      else return nullptr;
   }
}

static APIRET CfgDestroyColors (void)
{
   int i;

   for (i=0; i<COLOR_COUNT; i++)
   {
      if (CfgLocal.ColorArr[i])
         delete CfgLocal.ColorArr[i];
   }

   return NO_ERROR;
}

// ------------------------------------
//        Local / Hidden Devices
// ------------------------------------

static APIRET CfgLocalHiddenDeviceStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CfgLocal.pCurrentDeviceList->clear();
   *pBaseAddr = (long) &CfgLocal.CfgBuffLocalHiddenDevice;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgLocalDeviceStart (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CfgLocal.pCurrentDeviceList = &CfgLocal.LocalDevices;
   CHK (CfgLocalHiddenDeviceStart (pTableId, pBaseAddr, ppErrorText))
   return NO_ERROR;
}

static APIRET CfgHiddenDeviceStart (t_pchar pTableId, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CfgLocal.pCurrentDeviceList = &CfgLocal.HiddenDevices;
   CHK (CfgLocalHiddenDeviceStart (pTableId, pBaseAddr, ppErrorText))
   return NO_ERROR;
}

static APIRET CfgLocalHiddenDeviceSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   QString Device;

   Device = CfgLocal.CfgBuffLocalHiddenDevice.Device;
   *ppErrorText = nullptr;
   if (CfgLocal.pCurrentDeviceList->contains (Device))
        *ppErrorText = "This device figures twice in this table";
   else CfgLocal.pCurrentDeviceList->append (Device);

   *pBaseAddr = (long) &CfgLocal.CfgBuffLocalHiddenDevice;

   return NO_ERROR;
}

static APIRET CfgLocalHiddenDeviceEnd (t_pcchar *ppErrorText)
{
   CfgLocal.pCurrentDeviceList = nullptr;
   *ppErrorText = nullptr;
   return NO_ERROR;
}

APIRET CfgGetLocalDevices (QStringList **ppLocalDevices)
{
   *ppLocalDevices = &CfgLocal.LocalDevices;
   return NO_ERROR;
}

APIRET CfgGetHiddenDevices (QStringList **ppHiddenDevices)
{
   *ppHiddenDevices = &CfgLocal.HiddenDevices;
   return NO_ERROR;
}

// ------------------------------------
//         DeviceInfoCommands
// ------------------------------------

static APIRET CfgDeviceInfoCommandStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CfgLocal.DeviceInfoCommands.clear();
   *pBaseAddr = (long) &CfgLocal.CfgBuffDeviceInfoCommand;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgDeviceInfoCommandSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   QString Command;

   Command = CfgLocal.CfgBuffDeviceInfoCommand.Command;

   if (CfgLocal.DeviceInfoCommands.contains (Command))
   {
      *ppErrorText = "This command figures twice in the table DeviceInfoCommands";
   }
   else
   {
      CfgLocal.DeviceInfoCommands.append (Command);
      *ppErrorText = nullptr;
   }

   *pBaseAddr = (long) &CfgLocal.CfgBuffDeviceInfoCommand;

   return NO_ERROR;
}

static APIRET CfgDeviceInfoCommandEnd (t_pcchar *ppErrorText)
{
   *ppErrorText = nullptr;
   return NO_ERROR;
}

APIRET CfgGetDeviceInfoCommands (QStringList **ppDeviceInfoCommands)
{
   *ppDeviceInfoCommands = &CfgLocal.DeviceInfoCommands;
   return NO_ERROR;
}

// ------------------------------------
//           DlgAcquireField
// ------------------------------------

static APIRET CfgInitializeDlgAcquireFieldNames (void)
{
   if (CfgLocal.DlgAcquireFieldNames.count() == 0)
   {
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_SPLITFILESWITCH;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_SPLITFILESIZE;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_SPLITFILEUNIT;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_EWF_CASENUMBER;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_EWF_EVIDENCENUMBER;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_EWF_EXAMINER;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_EWF_DESCRIPTION;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_EWF_NOTES;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_USERFIELD;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_DEST_IMAGEDIRECTORY;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_DEST_IMAGEFILENAME;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_DEST_INFODIRECTORY;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_DEST_INFOFILENAME;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_HASH_CALC_MD5;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_HASH_CALC_SHA1;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_HASH_CALC_SHA256;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_HASH_VERIFY_SRC;
      CfgLocal.DlgAcquireFieldNames += CFG_DLGACQUIRE_HASH_VERIFY_DST;
   }
   return NO_ERROR;
}

static APIRET CfgDestroyDlgAcquireFields (void)
{
   for (int i=0; i<CfgLocal.DlgAcquireFields.count(); i++)
      delete CfgLocal.DlgAcquireFields[i];

   CfgLocal.DlgAcquireFields.clear();

   return NO_ERROR;
}

static APIRET CfgDlgAcquireFieldStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CHK (CfgInitializeDlgAcquireFieldNames ())
//   CHK (CfgDestroyDlgAcquireFields())
   *pBaseAddr = (long) &CfgLocal.CfgBuffDlgAcquireField;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgDlgAcquireFieldSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   t_pCfgBuffDlgAcquireField pBuff;
   t_pCfgDlgAcquireField     pField;
   bool                       Found = false;

   *ppErrorText = nullptr;
   pBuff = &CfgLocal.CfgBuffDlgAcquireField;

   if (CfgLocal.DlgAcquireFieldNames.contains(pBuff->FieldName))
   {
      for (int i=0; (i<CfgLocal.DlgAcquireFields.count()) && !Found; i++)
      {
         pField = CfgLocal.DlgAcquireFields[i];
         Found = (pField->FieldName == pBuff->FieldName);
      }
      if (!Found)
      {
         pField = new (t_CfgDlgAcquireField);
         pField->FieldName = pBuff->FieldName;
         pField->EwfField  = pField->FieldName.startsWith (CFG_DLGACQUIRE_EWF_FIELDID , Qt::CaseInsensitive);
         pField->HashField = pField->FieldName.startsWith (CFG_DLGACQUIRE_HASH_FIELDID, Qt::CaseInsensitive);
         pField->DstField  = pField->FieldName.startsWith (CFG_DLGACQUIRE_DST_FIELDID , Qt::CaseInsensitive);
         pField->DirField  = pField->FieldName.contains   (CFG_DLGACQUIRE_DIR_FIELDID , Qt::CaseInsensitive);
         CfgLocal.DlgAcquireFields.append (pField);
      }
      pField->EntryModeImage = pBuff->EntryModeImage;
      pField->EntryModeClone = pBuff->EntryModeClone;
      pField->DefaultValue   = pBuff->DefaultValue;
      pField->pLineEdit      = nullptr;
      pField->pCheckBox      = nullptr;
      pField->pComboBox      = nullptr;
   }
   else
   {
      *ppErrorText = "Unknown field name";
   }

   *pBaseAddr = (long) &CfgLocal.CfgBuffDlgAcquireField;

   return NO_ERROR;
}

static APIRET CfgDlgAcquireFieldEnd (t_pcchar *ppErrorText)
{
   bool Found = true;

   *ppErrorText = nullptr;

   for (int i=0; i<CfgLocal.DlgAcquireFieldNames.count() && Found; i++)
   {
      Found = false;
      for (int j=0; j<CfgLocal.DlgAcquireFields.count() && !Found; j++)
         Found = (CfgLocal.DlgAcquireFields[j]->FieldName.compare(CfgLocal.DlgAcquireFieldNames[i], Qt::CaseInsensitive) == 0);
      if (!Found)
      {
         LOG_INFO ("No entry for field %s found", QSTR_TO_PSZ(CfgLocal.DlgAcquireFieldNames[i]))
         *ppErrorText = "ENDTABLE statement reached, but some table entries are missing (see log output above)";
      }
   }

   return NO_ERROR;
}

APIRET CfgGetDlgAcquireFields (t_pCfgDlgAcquireFields *ppDlgAcquireFields)
{
   *ppDlgAcquireFields = &CfgLocal.DlgAcquireFields;
   return NO_ERROR;
}

// ------------------------------------
//            DlgAcquireRule
// ------------------------------------

static APIRET CfgDestroyDlgAcquireRules (void)
{
   for (int i=0; i<CfgLocal.DlgAcquireRules.count(); i++)
      delete CfgLocal.DlgAcquireRules[i];

   CfgLocal.DlgAcquireRules.clear();

   return NO_ERROR;
}

static APIRET CfgDlgAcquireRuleStart (t_pchar /*pTableId*/, long *pBaseAddr, t_pcchar *ppErrorText)
{
   CHK (CfgInitializeDlgAcquireFieldNames ())
   CHK (CfgDestroyDlgAcquireRules ())
   *pBaseAddr = (long) &CfgLocal.CfgBuffDlgAcquireRule;
   *ppErrorText = nullptr;

   return NO_ERROR;
}

static APIRET CfgDlgAcquireRuleSaveAndNext (long *pBaseAddr, t_pcchar *ppErrorText)
{
   t_pCfgBuffDlgAcquireRule pBuff;
   t_pCfgDlgAcquireRule     pRule;

   *ppErrorText = nullptr;
   pBuff = &CfgLocal.CfgBuffDlgAcquireRule;

   if (!CfgLocal.DlgAcquireFieldNames.contains(pBuff->TriggerFieldName))
   {
      printf ("\n%s", pBuff->TriggerFieldName);
      *ppErrorText = "Unknown trigger field name";
   }
   else if (!CfgLocal.DlgAcquireFieldNames.contains(pBuff->DestFieldName))
   {
      *ppErrorText = "Unknown destination field name";
   }
   else
   {
      pRule = new (t_CfgDlgAcquireRule);
      pRule->TriggerFieldName = pBuff->TriggerFieldName;
      pRule->DestFieldName    = pBuff->DestFieldName;
      pRule->Value            = pBuff->Value;
      CfgLocal.DlgAcquireRules.append (pRule);
   }

   *pBaseAddr = (long) &CfgLocal.CfgBuffDlgAcquireRule;

   return NO_ERROR;
}

static APIRET CfgDlgAcquireRuleEnd (t_pcchar *ppErrorText)
{
   *ppErrorText = nullptr;
   return NO_ERROR;
}

APIRET CfgGetDlgAcquireRules (t_pCfgDlgAcquireRules *ppDlgAcquireRules)
{
   *ppDlgAcquireRules = &CfgLocal.DlgAcquireRules;
   return NO_ERROR;
}

// ------------------------------------
//          Unicode translation
// ------------------------------------

APIRET ConfigTranslateUnicodeEscape (char *pSrc, QString *pDest)
{
   char *pCh;
   int    UnicodeChar;
   int    Digit;

   for (pCh=pSrc; *pCh != '\0'; pCh++)
   {
      if (*pCh == '\\')
      {
         pCh++;
         if (*pCh == '\\')
         {
            (*pDest) += *pCh;
         }
         else
         {
            UnicodeChar = 0;
            while (*pCh != ';')
            {
               if      (*pCh >= 'a')  Digit = *pCh - 'a' +10;
               else if (*pCh >= 'A')  Digit = *pCh - 'A' +10;
               else                   Digit = *pCh - '0';
               UnicodeChar = UnicodeChar*16 + Digit;
               pCh++;
            }
            (*pDest) += QChar (UnicodeChar);
         }
      }
      else
      {
         (*pDest) += *pCh;
      }
   }

   return NO_ERROR;
}

// ------------------------------------
//       Module initialisation
// ------------------------------------

APIRET CfgInit (void)
{
   t_pCfgData pCfgData;
   int         i;

   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_CFG_ONLY_TEMPLATE_GENERATED))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_CFG_INCOHERENCY_DETECTED))

   pCfgData = &CfgLocal.CfgData;
   memset (pCfgData, CONFIG_DUMMY_FILL, sizeof (t_CfgData));

   for (i=0; i<FONTOBJECT_COUNT; i++) CfgLocal.FontArr [i] = nullptr;
   for (i=0; i<COLOR_COUNT     ; i++) CfgLocal.ColorArr[i] = nullptr;

   return NO_ERROR;
}

APIRET CfgDeInit (void)
{
   CHK (CfgDestroyFonts            ())
   CHK (CfgDestroyColors           ())
   CHK (CfgDestroyDlgAcquireFields ())
   CHK (CfgDestroyDlgAcquireRules  ())

   return NO_ERROR;
}
