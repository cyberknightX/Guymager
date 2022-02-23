// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The main application window including menu and central
//                  widget
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

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "common.h"

#include "ewf.h"

#include "config.h"
#include "compileinfo.h"
#include "devicelistmodel.h"
#include "itemdelegate.h"
#include "mainwindow.h"
#include "threadscan.h"
#include "qtutil.h"
#include "infofield.h"
#include "table.h"
#include "dlgmessage.h"
#include "dlgautoexit.h"
#include "threadread.h"
#include "threadhash.h"
#include "threadcompress.h"
#include "threadwrite.h"
#include "runstats.h"


// -----------------------------
//            Constants
// -----------------------------

const int MAINWINDOW_WAIT_FOR_THREADS_TO_ABORT = 5000;

// -----------------------------
//  Table model for device list
// -----------------------------

static t_MainWindowColumn MainWindowColumnArr[] =
{
   { MAINWINDOW_COLUMN_SerialNr      , t_Device::GetSerialNumber  , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_LinuxDevice   , t_Device::GetLinuxDevice   , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_Model         , t_Device::GetModel         , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_NativePath    , t_Device::GetNativePath    , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_ByPath        , t_Device::GetByPath        , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_Interface     , t_Device::GetInterface     , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_State         , t_Device::GetState         , t_ItemDelegate::DISPLAYTYPE_STATE    },
   { MAINWINDOW_COLUMN_AddStateInfo  , t_Device::GetAddStateInfo  , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_Size          , t_Device::GetSizeHuman     , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_HiddenAreas   , t_Device::GetHiddenAreas   , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_BadSectors    , t_Device::GetBadSectorCount, t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_Progress      , t_Device::GetProgress      , t_ItemDelegate::DISPLAYTYPE_PROGRESS },
   { MAINWINDOW_COLUMN_AverageSpeed  , t_Device::GetAverageSpeed  , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_TimeRemaining , t_Device::GetRemaining     , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_FifoUsage     , t_Device::GetFifoStatus    , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_SectorSizeLog , t_Device::GetSectorSize    , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_SectorSizePhys, t_Device::GetSectorSizePhys, t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_CurrentSpeed  , t_Device::GetCurrentSpeed  , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_UserField     , t_Device::GetUserField     , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { MAINWINDOW_COLUMN_Examiner      , t_Device::GetExaminer      , t_ItemDelegate::DISPLAYTYPE_STANDARD },
   { nullptr                         , nullptr                    , 0                                    }
};

t_pMainWindowColumn MainWindowGetColumn (const char *pName)
{
   t_pMainWindowColumn pCol;

   for (pCol=&MainWindowColumnArr[0]; pCol->pName != nullptr; pCol++)
      if (strcasecmp (pCol->pName, pName) == 0)
         break;
   if (pCol->pName == nullptr)
      pCol = nullptr;

   return pCol;
}

bool MainWindowColumnExists (const char *pName)
{
   return (MainWindowGetColumn (pName) != nullptr);
}

t_DeviceListModel::t_DeviceListModel ()
{
   CHK_EXIT (ERROR_MAINWINDOW_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 Not initialised

t_DeviceListModel::t_DeviceListModel (t_pDeviceList pDeviceList)
   : QAbstractTableModel()
{
   t_pcCfgColumn       pColCfg;
   t_pMainWindowColumn pColDef;
   int                  Cols;
   t_ColAssoc           ColAssoc;

   poDeviceList = pDeviceList;

   Cols = CfgGetColumnCount ();
   for (int i =0; i<Cols; i++)
   {
      pColCfg = CfgGetColumn (i);
      if (pColCfg->ShowInMainTable)
      {
         pColDef = MainWindowGetColumn (pColCfg->Name);
         if (pColDef == nullptr)
            CHK_EXIT(ERROR_MAINWINDOW_UNKNOWN_COLUMN)

         ColAssoc.Name        = tr(pColCfg->Name);
         ColAssoc.pGetDataFn  = pColDef->pGetDataFn;
         ColAssoc.DisplayType = pColDef->DisplayType;
         ColAssoc.Alignment   = pColCfg->Alignment | Qt::AlignVCenter;
         ColAssoc.MinWidth    = pColCfg->MinWidth;
         ColAssocList.append (ColAssoc);
      }
   }
}

int t_DeviceListModel::rowCount (const QModelIndex & /*parent*/) const
{
   return poDeviceList->count();
}

int t_DeviceListModel::columnCount (const QModelIndex & /*parent*/) const
{
   return ColAssocList.count();
}

void t_DeviceListModel::SlotRefresh (void)
{
   static int LastCount=0;

   if (poDeviceList->count() != LastCount)
   {
      QAbstractTableModel::beginResetModel();
      QAbstractTableModel::endResetModel();
      LastCount = poDeviceList->count();
   }
   else
   {
      emit dataChanged (index(0,0), index(rowCount()-1, columnCount()-1) );
   }
}

//void t_DeviceListModel::SlotUpdate (void)
//{
//   emit dataChanged (index(0,0), index(rowCount()-1, columnCount()-1) );
//}

QVariant t_DeviceListModel::data(const QModelIndex &Index, int Role) const
{
   t_DeviceListModel::t_pGetDataFn pGetDataFn;
   t_pDevice                       pDev;
   QString                          Ret;
   QString                          Text;
   QVariant                         Value;

   if (!Index.isValid() && (Role == t_ItemDelegate::RowNrRole))  // For correct handling of the click events in the table
      return -1;

   if (Index.isValid() && (Index.column() < columnCount()) &&
                          (Index.row   () < rowCount   ()))
   {
      if (Index.column() >= ColAssocList.count())
         CHK_EXIT (ERROR_MAINWINDOW_INVALID_COLUMN)
      switch (Role)
      {
         case t_ItemDelegate::DisplayTypeRole: Value = ColAssocList[Index.column()].DisplayType; break;
         case t_ItemDelegate::MinColWidthRole: Value = ColAssocList[Index.column()].MinWidth;    break;
         case Qt::TextAlignmentRole          : Value = ColAssocList[Index.column()].Alignment;   break;

         case t_ItemDelegate::RowNrRole:
            Value = Index.row();
            break;
         case t_ItemDelegate::DeviceRole:
            pDev = poDeviceList->at(Index.row());
            Value = qVariantFromValue ((void *)pDev);
            break;
         case Qt::BackgroundRole:
            pDev = poDeviceList->at(Index.row());
            if (pDev->Local)
                 Value = QBrush (CONFIG_COLOR(COLOR_LOCALDEVICES));
            else switch (pDev->AddStateInfo.Color)
                 {
                    case 1 : Value = QBrush (CONFIG_COLOR(COLOR_ADDITIONALSTATE1)); break;
                    case 2 : Value = QBrush (CONFIG_COLOR(COLOR_ADDITIONALSTATE2)); break;
                    case 3 : Value = QBrush (CONFIG_COLOR(COLOR_ADDITIONALSTATE3)); break;
                    case 4 : Value = QBrush (CONFIG_COLOR(COLOR_ADDITIONALSTATE4)); break;
                    default: Value = QVariant();
                 }
            break;
         case Qt::DisplayRole:
            if (Index.column() >= ColAssocList.count())
               CHK_EXIT (ERROR_MAINWINDOW_INVALID_COLUMN)
            pDev = poDeviceList->at(Index.row());
            pGetDataFn = ColAssocList[Index.column()].pGetDataFn;
            Value = (*pGetDataFn) (pDev);
            break;
         case Qt::SizeHintRole:
            break;  // See t_ItemDelegate::sizeHint for column width calculation
         default:
            break;
      }
   }
   return Value;
}

QVariant t_DeviceListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (role != Qt::DisplayRole)
      return QVariant();

   if (orientation == Qt::Horizontal)
      return ColAssocList[section].Name;
   else
      return QString("Row %1").arg(section);
}

// -----------------------------
//          Main window
// -----------------------------

class t_MainWindowLocal
{
   public:
      t_DeviceList          *pDeviceList;
      t_DeviceListModel     *pDeviceListModel;
      QSortFilterProxyModel *pProxyModel;   // Inserted between pDeviceListModel and pTable, provides sorting functionality
      QWidget               *pCentralWidget;
      t_pTable               pTable;
      t_pInfoField           pInfoField;
      t_ThreadScan          *pThreadScan;
      QAction               *pActionRescan;
      QAction               *pActionAutoExit;
      t_ThreadScanWorker    *pScanWorker;
      QTimer                *pTimerRefresh;
      QTimer                *pTimerRunStats;
      t_pRunStats            pRunStats;
      bool                    AutoExit;
      bool                    AllAbortedByUser;
};

static APIRET MainWindowRegisterErrorCodes (void)
{
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_MAINWINDOW_CONSTRUCTOR_NOT_SUPPORTED))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_MAINWINDOW_INVALID_COLUMN))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_MAINWINDOW_INVALID_DATATYPE))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_MAINWINDOW_DEVICE_NOT_FOUND))
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_MAINWINDOW_UNKNOWN_COLUMN))

   return NO_ERROR;
}

APIRET t_MainWindow::CreateMenu (void)
{
   QMenuBar *pMenuBar = menuBar();
   QToolBar *pToolBar;
   QMenu    *pMenu;

   QTUTIL_SET_FONT (pMenuBar, FONTOBJECT_MENU)

   // Actions
   // -------
   pOwn->pActionRescan = new QAction (tr("&Rescan"), this);
   pOwn->pActionRescan->setShortcut  (tr("F5"));
   pOwn->pActionRescan->setToolTip   (tr("Rescan devices and update table"));

   // Menu
   // ----
   pMenu = pMenuBar->addMenu (tr("&Devices"));
   QTUTIL_SET_FONT (pMenu, FONTOBJECT_MENU)
   pMenu->addAction (pOwn->pActionRescan);
   pMenu->addAction (tr("Add special device", "Menu entry"), this, SLOT(SlotAddSpecialDevice()));

   pMenu = pMenuBar->addMenu (tr("&Misc", "Menu entry"));
   QTUTIL_SET_FONT (pMenu, FONTOBJECT_MENU)
   pOwn->pActionAutoExit = pMenu->addAction (tr("Exit after acquisitions completed"));
   pOwn->pActionAutoExit->setCheckable(true);
   pOwn->pActionAutoExit->setChecked ((CONFIG(AutoExit)!=0));
   pMenu->addAction (tr("Debug", "Menu entry"), this, SLOT(SlotDebug()));

   pMenu = pMenuBar->addMenu (tr("&Help", "Menu entry"));
   QTUTIL_SET_FONT (pMenu, FONTOBJECT_MENU)
   pMenu->addAction (tr("About &GUYMAGER", "Menu entry"), this, SLOT(SlotAboutGuymager()));  //lint !e64 !e119 Lint reports about type mismatches and
   pMenu->addAction (tr("About &Qt"      , "Menu entry"), this, SLOT(SlotAboutQt      ()));  //lint !e64 !e119 too many arguments, Lint is probably wrong


   // Toolbar
   // -------
   pToolBar = addToolBar (QString());
   pToolBar->addAction (pOwn->pActionRescan);
   QTUTIL_SET_FONT (pToolBar, FONTOBJECT_TOOLBAR)

   return NO_ERROR;
}


t_MainWindow::t_MainWindow(void)
{
   CHK_EXIT (ERROR_MAINWINDOW_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised

t_MainWindow::t_MainWindow (t_pDeviceList pDeviceList, QWidget *pParent, Qt::WindowFlags Flags)
   :QMainWindow (pParent, Flags)
{
   static bool Initialised = false;
   QString     Version;

   if (!Initialised)
   {
      CHK_EXIT (MainWindowRegisterErrorCodes())
      Initialised = true;
   }

   pOwn = new t_MainWindowLocal;
   pOwn->pDeviceList = pDeviceList;
   pOwn->AutoExit         = false;
   pOwn->AllAbortedByUser = false;
   pOwn->pRunStats        = new t_RunStats (pDeviceList);

   CHK_EXIT (CreateMenu ())

   Version = QString(pCompileInfoVersion).split(QRegExp("beta|-"))[0]; // Only take the main part of the version number
   setWindowTitle (QString ("GUYMAGER %1") .arg(Version));
//   setSizeGripEnabled (true);

   // Create models and view according do the following layering:
   //    TableView        -> GUI
   //    ProxyModel       -> Provides sorting
   //    DeviceListModel  -> Describes data access
   //    DeviceList       -> Contains data
   // -----------------------------------------------------------
   pOwn->pCentralWidget = new QWidget (this);
   QVBoxLayout *pLayout = new QVBoxLayout (pOwn->pCentralWidget);

   pOwn->pTable           = new t_Table (pOwn->pCentralWidget, this, pDeviceList);
   pOwn->pDeviceListModel = new t_DeviceListModel (pDeviceList);
   pOwn->pProxyModel      = new QSortFilterProxyModel (pOwn->pCentralWidget);
   pOwn->pProxyModel->setSourceModel (pOwn->pDeviceListModel);
   pOwn->pTable     ->setModel       (pOwn->pProxyModel);

   pOwn->pInfoField = new t_InfoField (pOwn->pCentralWidget, pDeviceList);

   pLayout->addWidget (pOwn->pTable);
   pLayout->addWidget (pOwn->pInfoField);

   CHK_QT_EXIT (connect (pOwn->pTable->horizontalHeader(), SIGNAL(sectionClicked            (int      )), pOwn->pTable    , SLOT(sortByColumn  (int      ))))
   CHK_QT_EXIT (connect (pOwn->pTable                    , SIGNAL(SignalDeviceSelected      (t_pDevice)), pOwn->pInfoField, SLOT(SlotShowInfo  (t_pDevice))))
   CHK_QT_EXIT (connect (pOwn->pTable                    , SIGNAL(SignalAllAcquisitionsEnded(void     )), this            , SLOT(SlotAutoExit  (void     ))))

   setCentralWidget (pOwn->pCentralWidget);

   // Start the device scan thread
   // ----------------------------
   pOwn->pThreadScan = new t_ThreadScan ();
   CHK_EXIT (pOwn->pThreadScan->Start (&pOwn->pScanWorker))
   CHK_QT_EXIT (connect (pOwn->pActionRescan, SIGNAL (triggered ()),          pOwn->pScanWorker, SLOT (SlotRescan       ())))
   CHK_QT_EXIT (connect (pOwn->pActionRescan, SIGNAL (triggered ()),                       this, SLOT (SlotScanStarted  ())))
   CHK_QT_EXIT (connect (pOwn->pScanWorker  , SIGNAL (SignalScanFinished (t_pDeviceList)), this, SLOT (SlotScanFinished (t_pDeviceList))))
   SlotScanStarted();
   QTimer::singleShot (300, pOwn->pScanWorker, SLOT(SlotRescan()));  // pOwn->pScanWorker->SlotRescan must not be called directly as it would be called from the
                                                                     // wrong thread! Using the signal/slot connection makes it behave correctly (see
                                                                     // Qt_AutoConnection, which is the default for connecting signals and slots).
   // Screen refresh timer
   // --------------------
   pOwn->pTimerRefresh = new QTimer(this);
   CHK_QT_EXIT (connect (pOwn->pTimerRefresh, SIGNAL(timeout()), this, SLOT(SlotRefresh())))
   pOwn->pTimerRefresh->start (CONFIG(ScreenRefreshInterval));

   // RunStats timer
   // --------------
   pOwn->pTimerRunStats = new QTimer(this); // Always create the timer (in order not to check if it exists before deleting it)
   if (pOwn->pRunStats->IsConfigured())
   {
      CHK_QT_EXIT (connect (pOwn->pTimerRunStats, SIGNAL(timeout()), this, SLOT(SlotUpdateRunStats())))
      pOwn->pTimerRunStats->start (CONFIG(RunStatsInterval)*1000);
      if (CONFIG(RunStatsInterval) > 5)                                  // Do not let the user wait too long for
         QTimer::singleShot(3000, this, SLOT(SlotUpdateRunStats()));     // the first output file to appear
   }
}

APIRET t_MainWindow::CheckDisconnectTimeout (void)  // Check for devices staying too long in
{                                                   // state disconnected
   t_pDevice pDev;

   for (int i=0; i < pOwn->pDeviceList->count(); i++)
   {
      pDev = pOwn->pDeviceList->at(i);
      if (pDev->DisconnectTimeoutReached())
      {
         LOG_INFO("[%s] Aborting acquisition because of disconnection timeout (SN: %s)", QSTR_TO_PSZ (pDev->LinuxDevice), QSTR_TO_PSZ(pDev->SerialNumber));
         pDev->Error.Set (t_Device::t_Error::DisconnectTimeout);
      }
   }
   return NO_ERROR;
}


void t_MainWindow::SlotRefresh (void)
{
   t_pDevice pDevice;

   pOwn->pDeviceListModel->SlotRefresh();
   CHK_EXIT (pOwn->pTable->GetDeviceUnderCursor (pDevice))
   pOwn->pInfoField->SlotShowInfo (pDevice);
   CHK_EXIT (CheckDisconnectTimeout())
}

void t_MainWindow::SlotRescan (void)
{
   if (pOwn->pActionRescan->isEnabled())
      pOwn->pActionRescan->trigger();
}

void t_MainWindow::SlotUpdateRunStats (void)
{
   pOwn->pRunStats->Update ();
}

void t_MainWindow::SlotAddSpecialDevice (void)
{
   static QDir  LastDir("/");
   QString      FileName;
   QFileInfo    FileInfo ;
   t_pDevice   pDevice;
   t_pDevice   pDeviceFound;
   FILE       *pFile = nullptr;
   bool         Error;
   qint64       Size;

   for (;;)
   {
      FileName = QFileDialog::getOpenFileName (this, tr("Open File"), LastDir.canonicalPath ());
      FileInfo.setFile (FileName);
      LastDir = FileInfo.dir();
      if (FileName.isNull())
         return;
      Size = FileInfo.size();
      if (Size <= 0)
      {
         LOG_INFO ("Qt thinks the file size for %s is %lld, trying seek to get it", QSTR_TO_PSZ(FileName), Size)
         pFile = fopen64 (QSTR_TO_PSZ(FileName), "r");
         Error = (pFile == nullptr);
         if (!Error)
            Error = (fseeko64 (pFile, 0, SEEK_END) != 0);
         if (!Error)
         {
            Size  = ftello64 (pFile);
            Error = (Size < 0);
         }

         if (Error)
            t_MessageBox::information (this, tr ("Invalid device", "Dialog title"), tr("The device or file cannot be selected because its size is unknown."), QMessageBox::Ok);
         else if (Size <= 0)
            t_MessageBox::information (this, tr ("Invalid device", "Dialog title"), tr("The device or file cannot be selected because it contains 0 bytes."), QMessageBox::Ok);
         if (Size <= 0)
            continue;
      }
      pDevice = new t_Device ("", FileName, tr("Manually added special device"), 512, 512, Size);
      pDevice->SpecialDevice = true;
      CHK_EXIT (pOwn->pDeviceList->MatchDevice (pDevice, pDeviceFound))
      if (pDeviceFound)
      {
         delete pDevice;
         t_MessageBox::information (this, tr ("Device already contained", "Dialog title"), tr("The selected file or device already is contained in the table."), QMessageBox::Ok);
         continue;
      }
      break;
   }
   LOG_INFO ("Adding special device %s with size %lld", QSTR_TO_PSZ(FileName), Size)

   pOwn->pDeviceList->append (pDevice);
   pOwn->pDeviceListModel->SlotRefresh();
   pOwn->pTable->resizeColumnsToContents();
}

APIRET t_MainWindow::RemoveSpecialDevice (t_pDevice pDevice)
{
   int i;

   i = pOwn->pDeviceList->indexOf (pDevice);
   if (i < 0)
      return ERROR_MAINWINDOW_DEVICE_NOT_FOUND;
   pOwn->pDeviceList->removeAt(i);

   pOwn->pDeviceListModel->SlotRefresh();
   pOwn->pTable->resizeColumnsToContents();

   return NO_ERROR;
}

void t_MainWindow::SlotScanStarted (void)
{
   pOwn->pActionRescan->setEnabled(false);
}


// SlotScanFinished provides us with the new device list. Now, our current device list needs to be updated. The rules are:
//  (1) For devices, that are in both lists:
//        Set the state of the device in the current list to Connected. Thus, devices that were temporarly disconnected
//        will become connected again.
//  (2) For devices, that only exist in the current list (but no longer in the new list):
//        a) delete from the current list if there was no acquisition or verifictaion running
//        b) switch to disconnected if there was an acquisition or source verification running, thus giving the user a 
//           chance to continue that acquisition on another device (for instance, if the hard disk had been unplugged from firewire 
//           and replugged to USB)
//        c) Do nothing if there's a verification running where only the image is verified
//  (3) For devices, that only exist in the new list:
//        Add to the current list
// Remark: Is it quite tricky to compare the devices from both lists if there are devices without a serial number. See
// t_DeviceList::MatchDevice for details.

void t_MainWindow::SlotScanFinished (t_pDeviceList pNewDeviceList)
{
   t_pDeviceList pDeviceList;
   t_pDevice     pDev, pNewDev;
   bool           NewDevice;
   int            i;

//   LOG_INFO ("%d devices found", pNewDeviceList->count())
   pDeviceList = pOwn->pDeviceList;
   for (i=0; i<pDeviceList->count(); i++)
   {
      pDev = pDeviceList->at (i);
      pDev->Checked = pDev->SpecialDevice; // Checked is used to remember which devices we have seen and which ones not. Treat 
      pDev->AddedNow=false;                // special devices as already seen, thus, they are not going to be removed from the list.
      if ((pDev->SpecialDevice) && (pDev->GetState() == t_Device::Launch)) // For special devices there is not such thing as devices changing
         pDev->SetState (t_Device::Acquire);                               // paths, so we change their state from Launch the Acquire here.
   }

   for (i=0; i<pNewDeviceList->count(); i++)
   {
      pNewDev = pNewDeviceList->at (i);
      CHK_EXIT (pDeviceList->MatchDevice (pNewDev, pDev))
//      if (pDev)
//      {
//         LOG_INFO ("device %s %s %s %llu matches %s %s %s %llu",
//            QSTR_TO_PSZ (pNewDev->LinuxDevice), QSTR_TO_PSZ (pNewDev->Model), QSTR_TO_PSZ (pNewDev->SerialNumber), pNewDev->Size,
//            QSTR_TO_PSZ (   pDev->LinuxDevice), QSTR_TO_PSZ (   pDev->Model), QSTR_TO_PSZ (   pDev->SerialNumber), pNewDev->Size)
//      }
//      else
//      {
//         LOG_INFO ("No match for device %s %s %s %llu", QSTR_TO_PSZ (pNewDev->LinuxDevice), QSTR_TO_PSZ (pNewDev->Model), QSTR_TO_PSZ (pNewDev->SerialNumber), pNewDev->Size)
//      }

      if (pDev)                           // Normally, if pDev has been found, it means that pNewDev already is known and should not be added another time. However,
           NewDevice = (pDev->AddedNow);  // function t_DeviceList::MatchDevice might be wrong in some cases and finds devices it shouldn't. This might happen
      else NewDevice = true;              // for devices of the same size and not having serial numbers. In order to prevent from these cases, we say that one device scan
                                          // very unlikely returns 2 entries for the same device (nobody unplugs and replugs a device that fast). So, if t_DeviceList::MatchDevice
                                          // returns a match with a device that has been added while we are processing this loop, we simply ignore the match and add the device anyway.
      if (!NewDevice)
      {
         pDev->Checked = true;
         pDev->NativePath  = pNewDev->NativePath;        // The device might have been connected to a
         pDev->ByPath      = pNewDev->ByPath;            // different port.
         pDev->Interface   = pNewDev->Interface;
         if (pDev->LinuxDevice != pNewDev->LinuxDevice)  // Linux may choose a new path when reconnecting the device
         {
            LOG_INFO ("Device previously accessible under %s moved to %s", QSTR_TO_PSZ(pDev->LinuxDevice), QSTR_TO_PSZ(pNewDev->LinuxDevice))
         }

         if ((pDev->GetState() == t_Device::Launch) ||
             (pDev->GetState() == t_Device::LaunchPaused))
         {
            if ((pDev->LinuxDevice != pNewDev->LinuxDevice) || (pDev->GetState() == t_Device::LaunchPaused))
            {
               LOG_INFO ("Just launched acquisition of %s: Device changed its path to %s, aborting.", QSTR_TO_PSZ (pDev->LinuxDevice), QSTR_TO_PSZ (pNewDev->LinuxDevice))
               pDev->Error.Set (t_Device::t_Error::DeviceMovedOnLaunch);
            }
            else
            {
               LOG_INFO ("Acquisition launch of %s confirmed", QSTR_TO_PSZ(pDev->LinuxDevice))
               pDev->SetState (t_Device::Acquire);
            }
         }
         else
         {
            if (pDev->GetState() == t_Device::AcquirePaused)
            {
               LOG_INFO ("Continuing acquisition of %s", QSTR_TO_PSZ(pDev->LinuxDevice))
               pDev->SetState (t_Device::Acquire);          // (1)
            }
            else if (pDev->GetState() == t_Device::VerifyPaused)
            {
               LOG_INFO ("Continuing verification of %s", QSTR_TO_PSZ(pDev->LinuxDevice))
               pDev->SetState (t_Device::Verify );          // (1)
            }
            pDev->LinuxDevice = pNewDev->LinuxDevice;
         }
      }
      else
      {
         pNewDev->Checked  = true;
         pNewDev->AddedNow = true;
         pNewDeviceList->removeAt (i--);                      // (3)
         pDeviceList->append (pNewDev);
      }
   }

   for (i=0; i<pDeviceList->count(); i++)
   {
      pDev = pDeviceList->at (i);
      if (!pDev->Checked)
      {
         switch (pDev->GetState())
         {
            case t_Device::Acquire      : pDev->SetState (t_Device::AcquirePaused);            // (2b)
                                          break;
            case t_Device::Verify       : if (pDev->Acquisition1.VerifySrc)
                                             pDev->SetState (t_Device::VerifyPaused );         // (2b)
                                          // else do nothing                                   // (2c)
                                          break;
            case t_Device::AcquirePaused: break;
            case t_Device::VerifyPaused : break;
            case t_Device::Launch       :
            case t_Device::LaunchPaused : LOG_INFO ("Just launched acquisition of %s: Device no longer available, aborting.", QSTR_TO_PSZ (pDev->LinuxDevice))
                                          pDev->Error.Set (t_Device::t_Error::DeviceMovedOnLaunch);
                                          break;
            default                     : pDeviceList->removeAt (i--);                         // (2a)
                                          delete pDev;
         }
      }
   }

   delete pNewDeviceList;

   pOwn->pDeviceListModel->SlotRefresh();
   pOwn->pTable->resizeColumnsToContents();
   pOwn->pActionRescan->setEnabled(true);
}

void t_MainWindow::closeEvent (QCloseEvent *pEvent)
{
   QMessageBox::StandardButton  Button;
   t_pDevice                   pDevice;
   bool                         AcquisitionsActive=false;
   int                          i;

   LOG_INFO ("User exits application")
   for (i=0; (i<pOwn->pDeviceList->count()) && (!AcquisitionsActive); i++)
   {
      pDevice = pOwn->pDeviceList->at(i);
      AcquisitionsActive = (pDevice->GetState() == t_Device::Acquire)       ||
                           (pDevice->GetState() == t_Device::Verify )       ||
                           (pDevice->GetState() == t_Device::Cleanup)       ||
                           (pDevice->GetState() == t_Device::Launch)        ||
                           (pDevice->GetState() == t_Device::LaunchPaused)  ||
                           (pDevice->GetState() == t_Device::AcquirePaused) ||
                           (pDevice->GetState() == t_Device::VerifyPaused);
   }

   if (AcquisitionsActive)
   {
      LOG_INFO ("Some acquisitions still active - ask user if he really wants to quit")
      Button = t_MessageBox::question (this, tr ("Exit GUYMAGER", "Dialog title"), tr("There are active acquisitions. Do you really want to abort them and quit?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (Button == QMessageBox::Yes)
      {  // stop acquisitions
         pOwn->AllAbortedByUser = true;
         LOG_INFO ("User confirms abortion in order to quit")
         CHK_EXIT (pOwn->pTable->AbortAllAcquisitions ())
         CHK_EXIT (QtUtilSleep (MAINWINDOW_WAIT_FOR_THREADS_TO_ABORT))
      }
      else
      {
         LOG_INFO ("User cancels exit request")
         pEvent->ignore();
      }
   }
}


void t_MainWindow::SlotDebug (void)
{
   t_pDevice pDevice;
   QString    DebugInfo;
   quint64    FifoAllocs, FifoFrees;
   qint64     FifoRemaining;
   qint64     FifoAllocated;

   // FIFO memory statistics
   // ----------------------
   CHK_EXIT (FifoGetStatistics (FifoAllocs, FifoFrees, FifoAllocated))
   FifoRemaining = FifoAllocs - FifoFrees;
   DebugInfo  = "FIFO Buffers";
   DebugInfo += QString ("\n   %1 allocated") .arg(FifoAllocs   , 10);
   DebugInfo += QString ("\n   %1 released" ) .arg(FifoFrees    , 10);
   DebugInfo += QString ("\n   %1 remaining") .arg(FifoRemaining, 10);
   DebugInfo += QString ("\n   %1 MB"       ) .arg((double)FifoAllocated / (1024.0*1024.0), 10, 'f', 1);

   // Thread messages
   // ---------------
   #define ADD_MESSAGE(pThread, Name) if (pDevice->pThread) DebugInfo += QString ("\n   %1: %2") .arg(Name, -13) .arg(pDevice->pThread->GetDebugMessage());

   for (int i=0; i<pOwn->pDeviceList->count(); i++)
   {
      pDevice = pOwn->pDeviceList->at(i);
      DebugInfo += QString ("\nThread messages for %1") .arg(pDevice->LinuxDevice);
      ADD_MESSAGE (pThreadRead, "Read")
      ADD_MESSAGE (pThreadHash, "Hash")
      for (int j=0; j<pDevice->ThreadCompressList.count(); j++)
         ADD_MESSAGE (ThreadCompressList.at(j), QString("Compress %1").arg(j))
      ADD_MESSAGE (pThreadWrite1, "Write1")
      if (pDevice->Duplicate)
          ADD_MESSAGE (pThreadWrite2, "Write2")
   }
   DebugInfo += QString ("\nThreadScan: %1") .arg(pOwn->pThreadScan->GetDebugMessage());

   CHK_EXIT (t_DlgMessage::Show (tr("Debug information"), DebugInfo, true))
}

void t_MainWindow::SlotAboutGuymager (void)
{
   const char *pLibGuyToolsVersionInstalled;

   t_Log::GetLibGuyToolsVersion (&pLibGuyToolsVersionInstalled);
   #if (ENABLE_LIBEWF)
      const char *pLibEwfVersionInstalled;
      QString      EwfRemark;

      pLibEwfVersionInstalled = (const char *) libewf_get_version ();
      if (CONFIG(EwfFormat) == t_File::AEWF)
         EwfRemark = tr("(not used as Guymager currently is configured to use its own EWF module)");
   #endif

   #if (ENABLE_LIBEWF)  // Do not factorise the code below, as Qt Linguist will have problems otherwise
      QMessageBox::about(this, tr("About GUYMAGER", "Dialog title"),
                               tr("GUYMAGER is a Linux-based forensic imaging tool\n\n"
                                  "Version: %1\n"
                                  "Version timestamp: %2\n"
                                  "Compiled with gcc %3\n"
                                  "Linked with libguytools %4\n"
                                  "Linked with libewf %5 %6")
                                  .arg(pCompileInfoVersion)
                                  .arg(pCompileInfoTimestampChangelog)
                                  .arg(__VERSION__)
                                  .arg(pLibGuyToolsVersionInstalled)
                                  .arg(pLibEwfVersionInstalled) .arg(EwfRemark)
                                  );
   #else
      QMessageBox::about(this, tr("About GUYMAGER", "Dialog title"),
                               tr("GUYMAGER is a Linux-based forensic imaging tool\n\n"
                                  "Version: %1\n"
                                  "Version timestamp: %2\n"
                                  "Compiled with gcc %3\n"
                                  "Linked with libguytools %4\n"
                                  "Compiled without libewf support")
                                  .arg(pCompileInfoVersion)
                                  .arg(pCompileInfoTimestampChangelog)
                                  .arg(__VERSION__)
                                  .arg(pLibGuyToolsVersionInstalled)
                                  );
   #endif
}

void t_MainWindow::SlotAboutQt (void)
{
   QMessageBox::aboutQt (this, tr("About Qt", "Dialog title"));
}

void t_MainWindow::SlotAutoExit (void)
{
   bool AutoExit;

   if (!pOwn->AllAbortedByUser)
   {
      if (pOwn->pActionAutoExit->isChecked())
      {
         LOG_INFO ("Showing autoexit countdown dialog (%d seconds)", CONFIG(AutoExitCountdown))
         CHK_EXIT (t_DlgAutoExit::Show (&AutoExit))
         if (AutoExit)
         {
            LOG_INFO ("Autoexit is becoming active, Guymager is exiting automatically now.")
            pOwn->AutoExit = true;
            emit SignalAutoExit();
         }
         else
         {
            LOG_INFO ("Autoexit countdown has been stopped. Guymager will continue to run.")
         }
      }
      else
      {
         LOG_INFO ("Signal \"all acquisitions ended\" received, but autoexit flag is off, doing nothing.")
      }
   }
}

bool t_MainWindow::AutoExit (void)
{
   return pOwn->AutoExit;
}

t_MainWindow::~t_MainWindow ()
{
   CHK_EXIT (pOwn->pThreadScan->Stop ())
   delete pOwn->pTimerRefresh;
   delete pOwn->pTimerRunStats;
   delete pOwn->pRunStats;
   delete pOwn->pTable;
   delete pOwn->pProxyModel;
   delete pOwn->pDeviceListModel;
   delete pOwn;
}

