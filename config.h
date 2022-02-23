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

#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG(Param)             ( CfgGetpData ()->Param)
#define CONFIG_COLOR(Color)       (*CfgGetpColor(Color))
#define CONFIG_FONT(FontObject)   (*CfgGetpFont (FontObject ))

typedef enum
{
   CFG_BOOLEANAUTO_FALSE = 0,
   CFG_BOOLEANAUTO_TRUE  = 1,
   CFG_BOOLEANAUTO_AUTO  = 2
} t_CfgBooleanAuto;

typedef enum
{
   CFG_STARTUPSIZE_STANDARD,
   CFG_STARTUPSIZE_FULLSCREEN,
   CFG_STARTUPSIZE_MAXIMIZED,
   CFG_STARTUPSIZE_MANUAL,
} t_CfgStartupSize;

typedef enum
{
   CFG_NUMBERSTYLE_LOCALE,
   CFG_NUMBERSTYLE_DECIMAL_COMMA,
   CFG_NUMBERSTYLE_DECIMAL_POINT
} t_CfgNumberStyle;

typedef enum
{
   CFG_ENTRYMODE_HIDE,
   CFG_ENTRYMODE_SHOWDEFAULT,
   CFG_ENTRYMODE_SHOWLAST
} t_CfgEntryMode;

const int CFG_MAX_LANGUAGE_LEN      =    8;
const int CFG_MAX_FONT_FAMILY_LEN   =   63;
const int CFG_MAX_PATH_LEN          = 1024;
const int CFG_MAX_TITLEID_LEN       =   31;
const int CFG_MAX_MENU_NAME_LEN     =  255;
const int CFG_MAX_LANGUAGE_CODE_LEN =    5;
const int CFG_MAX_FIELDNAME_LEN     =  255;
const int CFG_MAX_MISC_LEN          =  255;
const int CFG_MAX_COLUMNS           =   64;

class QColor; //lint !e763
class QFont;  //lint !e763

typedef enum
{
   FONTOBJECT_MENU=0,                 // must start with 0, as it is used for addressing an array
   FONTOBJECT_TOOLBAR,
   FONTOBJECT_TABLE,
   FONTOBJECT_INFOFIELD,
   FONTOBJECT_ACQUISITION_DIALOGS,
   FONTOBJECT_MESSAGE_DIALOGS,
   FONTOBJECT_DIALOG_DATA,

   FONTOBJECT_COUNT
} t_CfgFontObject;

typedef enum
{
   COLOR_LOCALDEVICES=0,
   COLOR_ADDITIONALSTATE1,
   COLOR_ADDITIONALSTATE2,
   COLOR_ADDITIONALSTATE3,
   COLOR_ADDITIONALSTATE4,
   COLOR_STATE_IDLE,
   COLOR_STATE_QUEUED,
   COLOR_STATE_ACQUIRE,
   COLOR_STATE_ACQUIRE_PAUSED,
   COLOR_STATE_VERIFY,
   COLOR_STATE_VERIFY_PAUSED,
   COLOR_STATE_CLEANUP,
   COLOR_STATE_FINISHED,
   COLOR_STATE_FINISHED_BADVERIFY,
   COLOR_STATE_FINISHED_DUPLICATE_FAILED,
   COLOR_STATE_ABORTED_USER,
   COLOR_STATE_ABORTED_OTHER,

   COLOR_COUNT,
   COLOR_DEFAULT=-1  // Not really a color, but used for specifying, that a widget should switch to its default color (grey for a button, for instance)
} t_CfgColor;

typedef enum
{
   SCANMETHOD_LIBPARTED=0,
   SCANMETHOD_DBUSHAL,
   SCANMETHOD_DBUSDEVKIT,
   SCANMETHOD_LIBUDEV,

   SCANMETHOD_COUNT
} t_CfgScanMethod;

typedef enum
{
   EWFNAMING_OLD,
   EWFNAMING_FTK
} t_CfgEwfNaming;

const int LIBEWF_COMPRESSION_EMPTY = 100; // Take care not to collide with LIBEWF definitions LIBEWF_COMPRESSION_FAST, LIBEWF_COMPRESSION_BEST etc.


// ATTENTION: The data of the following structure is filled in by the CFG tool. As this
// one doesn't know about 'bool' (it's written in ANSI C), bool MUST not be used here.
// ------------------------------------------------------------------------------------
#undef  bool
#define bool "Do not use bool, take int instead"
typedef struct
{
   char                 Language[CFG_MAX_LANGUAGE_LEN+1];
   int                  CheckRootRights;

   t_CfgStartupSize     StartupSize;
   int                  StartupSizeManualX;
   int                  StartupSizeManualY;
   int                  StartupSizeManualDx;
   int                  StartupSizeManualDy;

   t_CfgStartupSize     FileDialogSize;
   int                  FileDialogSizeManualDx;
   int                  FileDialogSizeManualDy;

   t_CfgNumberStyle     NumberStyle;
   int                  ScreenRefreshInterval;
   int                  UseFileDialogFromQt;
   char                 UserFieldName          [CFG_MAX_MISC_LEN+1];
   char                 AdditionalStateInfoName[CFG_MAX_MISC_LEN+1];

   int                  WarnAboutImageSize;
   int                  WarnAboutSegmentFileCount;
   int                  DeleteAbortedImageFiles;

   int                  AutoExit;
   int                  AutoExitCountdown;

   t_CfgScanMethod      DeviceScanMethod;
   int                  ScanInterval;
   char                 CommandGetSerialNumber[CFG_MAX_PATH_LEN+1];
   int                  ForceCommandGetSerialNumber;
   char                 CommandGetAddStateInfo[CFG_MAX_PATH_LEN+1];
   char                 CommandAcquisitionEnd [CFG_MAX_PATH_LEN+1];
   int                  QueryDeviceMediaInfo;
   char                 RunStatsTemplateActive[CFG_MAX_PATH_LEN+1];
   char                 RunStatsTemplateEnded [CFG_MAX_PATH_LEN+1];
   char                 RunStatsOutput        [CFG_MAX_PATH_LEN+1];
   int                  RunStatsInterval;

   int                  DirectIO;

   int                  DefaultFormat;
   int                  InfoFieldsForDd;
   int                  EwfFormat;
   int                  EwfCompression;
   double               EwfCompressionThreshold;
   t_CfgEwfNaming       EwfNaming;
   int                  AffEnabled;
   int                  AffCompression;
   int                  AffMarkBadSectors;
   char                 SpecialFilenameChars[CFG_MAX_MISC_LEN+1];
   int                  CalcImageFileMD5;
   int                  DuplicateImage;
   int                  DirectoryFieldEditing;
   int                  AllowPathInFilename;
   int                  ConfirmDirectoryCreation;
   int                  AvoidEncaseProblems;
   int                  AvoidCifsProblems;

   int                  FifoBlockSizeDD;
   int                  FifoBlockSizeEWF;
   int                  FifoBlockSizeAFF;
   int                  FifoMaxMem;
   int                  FifoMemoryManager;
   int                  UseSeparateHashThread;
   int                  CompressionThreads;
   int                  BadSectorLogThreshold;
   int                  BadSectorLogModulo;
   int                  LimitJobs;
   int                  JobMaxBadSectors;
   int                  JobDisconnectTimeout;
   int                  LocalHiddenDevicesUseRegExp;

   int                  SignalHandling;
   int                  WriteToDevNull;
   int                  UseMemWatch;
   int                  VerboseLibewf;
   int                  CheckEwfData;
} t_CfgData, *t_pCfgData;
#undef bool


typedef struct
{
   char      Code      [CFG_MAX_LANGUAGE_CODE_LEN+1];
   char      AsciiName [CFG_MAX_MENU_NAME_LEN    +1];
} t_CfgBuffLanguage, *t_pCfgBuffLanguage;

typedef struct
{
   t_CfgFontObject Object;
   char            Family[CFG_MAX_FONT_FAMILY_LEN+1];
   int             Size;
   int             Weight;
   int             Italic;
} t_CfgBuffFont, *t_pCfgBuffFont;

typedef struct
{
   char Name[CFG_MAX_FIELDNAME_LEN+1];
   int  Alignment;
   int  MinWidth;
   int  ShowInMainTable;
   int  ShowInCloneTable;
} t_CfgColumn, *t_pCfgColumn;
typedef t_CfgColumn const *t_pcCfgColumn;

typedef struct
{
   t_CfgColor Color;
   int        R;
   int        G;
   int        B;
} t_CfgBuffColor, *t_pCfgBuffColor;

typedef struct
{
   char Device [CFG_MAX_PATH_LEN+1];
} t_CfgBuffLocalHiddenDevice, *t_pCfgBuffLocalHiddenDevice;

typedef struct
{
   char Command [CFG_MAX_PATH_LEN+1];
} t_CfgBuffDeviceInfoCommand, *t_pCfgBuffDeviceInfoCommand;


// Special constants
// -----------------
#define CONFIG_LIMITJOBS_OFF  -2
#define CONFIG_LIMITJOBS_AUTO -1


// DlgAcquire fields and rules
// ---------------------------

#define CFG_DLGACQUIRE_EWF_FIELDID        "Ewf"
#define CFG_DLGACQUIRE_HASH_FIELDID       "Hash"
#define CFG_DLGACQUIRE_HASHCALC_FIELDID   "HashCalc"
#define CFG_DLGACQUIRE_HASHVERIFY_FIELDID "HashVerify"
#define CFG_DLGACQUIRE_DST_FIELDID        "Dest"
#define CFG_DLGACQUIRE_DIR_FIELDID        "Directory"

#define CFG_DLGACQUIRE_SPLITFILESWITCH     QT_TRANSLATE_NOOP("t_DlgAcquire", "SplitFileSwitch"   )
#define CFG_DLGACQUIRE_SPLITFILESIZE       QT_TRANSLATE_NOOP("t_DlgAcquire", "SplitFileSize"     )
#define CFG_DLGACQUIRE_SPLITFILEUNIT       QT_TRANSLATE_NOOP("t_DlgAcquire", "SplitFileUnit"     )
#define CFG_DLGACQUIRE_EWF_CASENUMBER      QT_TRANSLATE_NOOP("t_DlgAcquire", "EwfCaseNumber"     )
#define CFG_DLGACQUIRE_EWF_EVIDENCENUMBER  QT_TRANSLATE_NOOP("t_DlgAcquire", "EwfEvidenceNumber" )
#define CFG_DLGACQUIRE_EWF_EXAMINER        QT_TRANSLATE_NOOP("t_DlgAcquire", "EwfExaminer"       )
#define CFG_DLGACQUIRE_EWF_DESCRIPTION     QT_TRANSLATE_NOOP("t_DlgAcquire", "EwfDescription"    )
#define CFG_DLGACQUIRE_EWF_NOTES           QT_TRANSLATE_NOOP("t_DlgAcquire", "EwfNotes"          )
#define CFG_DLGACQUIRE_USERFIELD                                             "UserField"
#define CFG_DLGACQUIRE_DEST_IMAGEDIRECTORY QT_TRANSLATE_NOOP("t_DlgAcquire", "DestImageDirectory")
#define CFG_DLGACQUIRE_DEST_IMAGEFILENAME  QT_TRANSLATE_NOOP("t_DlgAcquire", "DestImageFilename" )
#define CFG_DLGACQUIRE_DEST_INFODIRECTORY  QT_TRANSLATE_NOOP("t_DlgAcquire", "DestInfoDirectory" )
#define CFG_DLGACQUIRE_DEST_INFOFILENAME   QT_TRANSLATE_NOOP("t_DlgAcquire", "DestInfoFilename"  )
#define CFG_DLGACQUIRE_HASH_CALC_MD5       QT_TRANSLATE_NOOP("t_DlgAcquire", "HashCalcMD5"       )
#define CFG_DLGACQUIRE_HASH_CALC_SHA1      QT_TRANSLATE_NOOP("t_DlgAcquire", "HashCalcSHA1"      )
#define CFG_DLGACQUIRE_HASH_CALC_SHA256    QT_TRANSLATE_NOOP("t_DlgAcquire", "HashCalcSHA256"    )
#define CFG_DLGACQUIRE_HASH_VERIFY_SRC     QT_TRANSLATE_NOOP("t_DlgAcquire", "HashVerifySrc"     )
#define CFG_DLGACQUIRE_HASH_VERIFY_DST     QT_TRANSLATE_NOOP("t_DlgAcquire", "HashVerifyDst"     )

typedef struct
{
   char            FieldName   [CFG_MAX_FIELDNAME_LEN+1];
   t_CfgEntryMode  EntryModeImage;
   t_CfgEntryMode  EntryModeClone;
   char            DefaultValue[CFG_MAX_MISC_LEN     +1];
} t_CfgBuffDlgAcquireField, *t_pCfgBuffDlgAcquireField;

typedef struct
{
   char TriggerFieldName[CFG_MAX_FIELDNAME_LEN+1];
   char DestFieldName   [CFG_MAX_FIELDNAME_LEN+1];
   char Value           [CFG_MAX_MISC_LEN     +1];
} t_CfgBuffDlgAcquireRule, *t_pCfgBuffDlgAcquireRule;

class QCheckBox;
class QComboBox;
class QLabel;
class t_DlgAcquireLineEdit;
class t_CfgDlgAcquireField
{
   public:
      QString                FieldName;
      t_CfgEntryMode         EntryModeImage;
      t_CfgEntryMode         EntryModeClone;
      QString                DefaultValue;
      QString                LastEnteredValue; // not set by config, used by DlgAcquire
      bool                   EwfField;         // This field is part of the EWF fields
      bool                   HashField;        // This field is part of the hash calculation/verification fields
      bool                   DstField;         // This field is part of the fields for entering the image/info destination
      bool                   DirField;         // This is a directory field with a browse button
      t_DlgAcquireLineEdit *pLineEdit;
      QCheckBox            *pCheckBox;
      QComboBox            *pComboBox;
      QLabel               *pLabel;
}; //lint !e1553
typedef t_CfgDlgAcquireField       *t_pCfgDlgAcquireField;
typedef QList<t_pCfgDlgAcquireField> t_CfgDlgAcquireFields;
typedef t_CfgDlgAcquireFields      *t_pCfgDlgAcquireFields;

class t_CfgDlgAcquireRule
{
   public:
      QString TriggerFieldName;
      QString DestFieldName;
      QString Value;
}; //lint !e1553
typedef t_CfgDlgAcquireRule        *t_pCfgDlgAcquireRule;
typedef QList<t_pCfgDlgAcquireRule> t_CfgDlgAcquireRules;
typedef t_CfgDlgAcquireRules       *t_pCfgDlgAcquireRules;

QFont        *CfgGetpFont    (t_CfgFontObject Object);
const QColor *CfgGetpColor   (t_CfgColor      Color );
APIRET CfgGetLocalDevices        (QStringList **ppLocalDevices);
APIRET CfgGetHiddenDevices       (QStringList **ppLocalDevices);
APIRET CfgGetDeviceInfoCommands  (QStringList **ppDeviceInfoCommands);
APIRET CfgGetDlgAcquireFields    (t_pCfgDlgAcquireFields *ppDlgAcquireFields);
APIRET CfgGetDlgAcquireRules     (t_pCfgDlgAcquireRules  *ppDlgAcquireRules);

QString CfgGetHostName   (void);
QString CfgGetMacAddress (void);

int           CfgGetColumnCount (void);
t_pcCfgColumn CfgGetColumn      (int i);

APIRET CfgInit        (void);
APIRET CfgDeInit      (void);

t_pCfgData CfgGetpData(void);

APIRET CfgGetLogFileName    (t_pcchar *ppLogFileName, bool *pDefaultUsed);
APIRET CfgGetCfgFileName    (t_pcchar *ppCfgFileName, bool *pDefaultUsed);
APIRET CfgReadConfiguration (t_pcchar   pCfgFileName);

APIRET ConfigTranslateUnicodeEscape (char *pSrc, QString *pDest);

void CfgFontPrint (void);

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_CFG_ONLY_TEMPLATE_GENERATED = ERROR_BASE_CFG + 1,
   ERROR_CFG_INCOHERENCY_DETECTED
};


#endif

