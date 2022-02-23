// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The table widget (central widget of the application)
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

// Since version 0.7.x, Guymager includes the JobQueue feature. The idea and
// initial source code for this are a contribution of Erwin van Eijk.

#include <unistd.h>
#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

#include "common.h"
#include "config.h"
#include "threadread.h"
#include "threadwrite.h"
#include "threadhash.h"
#include "threadcompress.h"
#include "dlgmessage.h"
#include "dlgacquire.h"
#include "itemdelegate.h"
#include "table.h"
#include "mainwindow.h"
#include "compileinfo.h"
#include "dlgabort.h"
#include "file.h"
#include "qtutil.h"
#include "util.h"
#include "infofield.h"

#include "toolconstants.h"
#include "toolsysinfo.h"


class t_TableLocal
{
   public:
      t_pDeviceList      pDeviceList;
      t_MainWindow      *pMainWindow;
      QAction           *pActionAcquire;
      QAction           *pActionClone;
      QAction           *pActionAbort;
      QAction           *pActionDeviceInfo;
      QAction           *pActionRemoveSpecialDevice;
      QQueue<t_pDevice>   JobQueue;
      bool                SlowDownAcquisitions;
};

t_Table::t_Table ()
{
   CHK_EXIT (ERROR_TABLE_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 not initialised

t_Table::t_Table (QWidget *pParent, t_MainWindow *pMainWindow, t_pDeviceList pDeviceList)
   :QTableView (pParent)
{
//   setStyleSheet("QTableView {background-color: yellow;}");

   QTUTIL_SET_FONT (this, FONTOBJECT_TABLE)

   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_INVALID_ACTION                ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_THREADREAD_ALREADY_RUNNING    ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_THREADHASH_ALREADY_RUNNING    ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_THREADWRITE_ALREADY_RUNNING   ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_THREADCOMPRESS_ALREADY_RUNNING))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_THREADREAD_DOESNT_EXIT        ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_FIFO_EXISTS                   ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_CONSTRUCTOR_NOT_SUPPORTED     ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_INVALID_FORMAT                ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_INVALID_DIALOG_CODE           ))
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_TABLE_INVALID_FREE_HANDLE           ))

   setItemDelegate      (new t_ItemDelegate (this));
   setSelectionBehavior (QAbstractItemView::SelectRows     );
   setSelectionMode     (QAbstractItemView::SingleSelection);

   pOwn = new t_TableLocal;
   pOwn->pDeviceList          = pDeviceList;
   pOwn->pMainWindow          = pMainWindow;
   pOwn->SlowDownAcquisitions = false;

   verticalHeader()->hide();
   #if (QT_VERSION >= 0x050000)
      horizontalHeader()->setSectionsClickable  (true);
   #else
      horizontalHeader()->setClickable          (true);
   #endif
   horizontalHeader()->setSortIndicatorShown (true);
   setHorizontalScrollMode (QAbstractItemView::ScrollPerPixel);
   setVerticalScrollMode   (QAbstractItemView::ScrollPerPixel);

   pOwn->pActionAcquire             = new QAction(tr("Acquire image"        , "Context menu"), this);  addAction (pOwn->pActionAcquire            );
   pOwn->pActionClone               = new QAction(tr("Clone device"         , "Context menu"), this);  addAction (pOwn->pActionClone              );
   pOwn->pActionAbort               = new QAction(tr("Abort"                , "Context menu"), this);  addAction (pOwn->pActionAbort              );
   pOwn->pActionDeviceInfo          = new QAction(tr("Info"                 , "Context menu"), this);  addAction (pOwn->pActionDeviceInfo         );
   pOwn->pActionRemoveSpecialDevice = new QAction(tr("Remove special device", "Context menu"), this);  addAction (pOwn->pActionRemoveSpecialDevice);

   CHK_QT_EXIT (connect (this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(SlotMouseClick(const QModelIndex &))))

//   pOwn->pTable->setContextMenuPolicy (Qt::ActionsContextMenu);
}

t_Table::~t_Table ()
{
   delete pOwn;
}

APIRET t_Table::GetDeviceUnderCursor (t_pDevice &pDevice)
{
   QModelIndex Index;
   int         Row;
   int         Count;

   pDevice = nullptr;
   QModelIndexList IndexList = selectionModel()->selectedRows();
   Count = IndexList.count();
   switch (Count)
   {
      case 0:  break;
      case 1:  Index = IndexList.first();
               Row = model()->data(Index, t_ItemDelegate::RowNrRole).toInt();
               pDevice = pOwn->pDeviceList->at(Row);
               break;
      default: LOG_ERROR ("Strange, several rows seem to be selected.")
   }

   return NO_ERROR;
}

APIRET t_Table::ContextMenu (const QPoint &GlobalPos)
{
   QAction     *pAction;
   QAction       LocalIndicator        (tr("Local Device - cannot be acquired"                   ), this);
   QAction       CloneIndicator        (tr("Device used for cloning - cannot be acquired"        ), this);
   QAction       AddStateInfoIndicator (tr("Cannot be acquired - see additional state info field"), this);
   QMenu         Menu;
   QPoint        Pos;
   t_pDevice    pDevice;
   int           CorrectedY;
   int           Row;
   QModelIndex   Index;
   bool          RunningOrQueued;

   QTUTIL_SET_FONT (&Menu, FONTOBJECT_MENU)

   Pos = mapFromGlobal (GlobalPos);                     // The event coordinates are absolute screen coordinates, map them withb this widget
   CorrectedY = Pos.y() - horizontalHeader()->height(); // QTableView::rowAt seems to have a bug: It doesn't include the header height in the calculations. So, we correct it here.
   Pos.setY (CorrectedY);

   Index = indexAt (Pos);
   selectRow (Index.row());
   Row = model()->data(Index, t_ItemDelegate::RowNrRole).toInt();

   if ((Row < 0) || (Row >= pOwn->pDeviceList->count())) // User clicked in the empty area
      return NO_ERROR;

   pDevice = pOwn->pDeviceList->at(Row);
//   LOG_INFO ("Context global %d -- widget %d -- corrected %d -- index %d", GlobalPos.y(), Pos.y(), CorrectedY, RowIndex);

   RunningOrQueued = pDevice->pThreadRead   ||  pDevice->pThreadWrite1 ||
                     pDevice->pThreadWrite2 || (pDevice->GetState()==t_Device::Queued);

   pOwn->pActionAcquire->setEnabled (!RunningOrQueued);
   pOwn->pActionClone  ->setEnabled (!RunningOrQueued);
   pOwn->pActionAbort  ->setEnabled ( RunningOrQueued);

   if (pDevice->Local)
   {
      LocalIndicator.setEnabled (false);
      Menu.addAction (&LocalIndicator);
      Menu.addSeparator ();
   }
   else if (pOwn->pDeviceList->UsedAsCloneDestination(pDevice))
   {
      CloneIndicator.setEnabled (false);
      Menu.addAction (&CloneIndicator);
      Menu.addSeparator ();
   }
   else if (!pDevice->AddStateInfo.CanBeAcquired)
   {
      AddStateInfoIndicator.setEnabled (false);
      Menu.addAction (&AddStateInfoIndicator);
      Menu.addSeparator ();
   }
   else
   {
      Menu.addAction (pOwn->pActionAcquire);
      Menu.addAction (pOwn->pActionClone  );
      Menu.addAction (pOwn->pActionAbort  );
   }
   Menu.addAction (pOwn->pActionDeviceInfo);

   if (pDevice->SpecialDevice)
   {
      pOwn->pActionRemoveSpecialDevice->setEnabled (!RunningOrQueued);
      Menu.addAction (pOwn->pActionRemoveSpecialDevice);
   }

//   StartRc = NO_ERROR;
   pAction = Menu.exec (GlobalPos);
   if      (pAction == pOwn->pActionAcquire            ) CHK_EXIT (ShowAcquisitionDialog (pDevice, false))
   else if (pAction == pOwn->pActionClone              ) CHK_EXIT (ShowAcquisitionDialog (pDevice, true ))
   else if (pAction == pOwn->pActionAbort              ) CHK_EXIT (AbortAcquisition      (pDevice))
   else if (pAction == pOwn->pActionDeviceInfo         ) CHK_EXIT (ShowDeviceInfo        (pDevice))
   else if (pAction == pOwn->pActionRemoveSpecialDevice) CHK_EXIT (pOwn->pMainWindow->RemoveSpecialDevice(pDevice))
   else
   {
//      CHK_EXIT (ERROR_TABLE_INVALID_ACTION)
   }

//   if (StartRc != NO_ERROR)
//      pDevice->Error.Set(t_Device::t_Error::AcquisitionStartFailed);
   return NO_ERROR;
}

void t_Table::contextMenuEvent (QContextMenuEvent *pEvent)
{
   CHK_EXIT (ContextMenu (pEvent->globalPos()))
}

void t_Table::mouseDoubleClickEvent (QMouseEvent *pEvent)
{
   CHK_EXIT (ContextMenu (pEvent->globalPos()))
}

void t_Table::SlotMouseClick (const QModelIndex & Index)
{
   int Row;

   selectRow (Index.row());
   Row = model()->data (Index, t_ItemDelegate::RowNrRole).toInt();
   emit (SignalDeviceSelected (pOwn->pDeviceList->at(Row)));
}


APIRET t_Table::ShowDeviceInfo (t_pcDevice pDevice)
{
   QString Info;

   LOG_INFO ("[%s] Info requested", QSTR_TO_PSZ(pDevice->LinuxDevice))
   pOwn->SlowDownAcquisitions = true;
   CHK (t_Info::GetDeviceInfo (pDevice, true, Info))
   pOwn->SlowDownAcquisitions = false;
   CHK (t_DlgMessage::Show (tr("Device info", "Dialog title"), Info, true, this))

   return NO_ERROR;
}


// ----------------------------------------------------------------------------------------------------------------
//
//                                                     Acquistion start
//
// ----------------------------------------------------------------------------------------------------------------

APIRET t_Table::InfoAcquisitionStart (t_pAcquisition pAcquisition)
{
   const char *pLibGuyToolsVersionInstalled;
   QString      FormatDescription;
   QString      FormatExtension;
   QString      Str;
   QString      Uname;
   t_UtilMem    Hostname(200);
   t_UtilMem    Domainname(200);
   char       *pHostname;
   char       *pDomainname;

   CHK (pAcquisition->Info.Create())
   CHK (pAcquisition->Info.Title   (tr("GUYMAGER ACQUISITION INFO FILE", "Info file")))

   t_Log::GetLibGuyToolsVersion (&pLibGuyToolsVersionInstalled);
   CHK (ToolSysInfoUname (Uname))
   pHostname   = (char *) Hostname.GetpMem();
   pDomainname = (char *) Domainname.GetpMem();
   if (gethostname (pHostname, Hostname.Size()) != 0)
      Hostname.Set  ("-- not available --");
   if (getdomainname (pDomainname, Domainname.Size()) != 0)
      Domainname.Set("-- not available --");

   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.Title   (tr("Guymager", "Info file")))
   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.AddRow (tr("Version:: %1"          , "Info file") .arg(pCompileInfoVersion           )))
   CHK (pAcquisition->Info.AddRow (tr("Version timestamp:: %1", "Info file") .arg(pCompileInfoTimestampChangelog)))
   CHK (pAcquisition->Info.AddRow (tr("Compiled with:: %1 %2" , "Info file") .arg("gcc") .arg(__VERSION__)))
   #if (ENABLE_LIBEWF)
      QString EwfRemark;
      if (CONFIG(EwfFormat) == t_File::AEWF)
         EwfRemark = tr("(not used as Guymager is configured to use its own EWF module)", "Info file");
      CHK (pAcquisition->Info.AddRow (tr("libewf version:: %1 %2" , "Info file") .arg((const char *) libewf_get_version()) .arg(EwfRemark)))
   #else
      CHK (pAcquisition->Info.AddRow (tr("Using Guymager's own EWF module", "Info file")));
   #endif
   CHK (pAcquisition->Info.AddRow (tr("libguytools version:: %1"  , "Info file") .arg(pLibGuyToolsVersionInstalled)))
   CHK (pAcquisition->Info.AddRow (tr("Host name:: %1"            , "Info file") .arg(pHostname)))
   CHK (pAcquisition->Info.AddRow (tr("Domain name:: %1"          , "Info file") .arg(pDomainname)))
   CHK (pAcquisition->Info.AddRow (tr("System:: %1"               , "Info file") .arg(Uname)))
   CHK (pAcquisition->Info.WriteTable ());

   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.Title   (tr("Device information", "Info file")))
   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.WriteDeviceInfo ())

   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.WriteLn (tr("Hidden areas: %1", "Info file") .arg(t_Device::GetHiddenAreas(pAcquisition->pDevice).toString())))

   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.Title   (tr("Acquisition", "Info file")))
   CHK (pAcquisition->Info.WriteLn ())
   CHK (t_File::GetFormatDescription (pAcquisition->pDevice,           FormatDescription))
   CHK (t_File::GetFormatExtension   (pAcquisition->pDevice, nullptr, &FormatExtension))
   CHK (pAcquisition->Info.AddRow (tr("Linux device:: %1"    , "Info file").arg(pAcquisition->pDevice->LinuxDevice)))
   CHK (pAcquisition->Info.AddRow (tr("Device size:: %1 (%2)", "Info file").arg(pAcquisition->pDevice->Size) .arg(t_Device::GetSizeHuman(pAcquisition->pDevice).toString())))
   Str = tr("Format:: %1", "Info file") .arg(FormatDescription);
   if (!FormatExtension.isEmpty())
      Str += tr(" - file extension is %1", "Info file") .arg(FormatExtension);
   CHK (pAcquisition->Info.AddRow (Str))
   if ((pAcquisition->Format != t_File::DD) || CONFIG (InfoFieldsForDd))
   {
      CHK (pAcquisition->Info.AddRow (tr("Image meta data", "Info file")))
      CHK (pAcquisition->Info.AddRow ("   " + t_DlgAcquire::tr("EwfCaseNumber"    ) + QString(":: %1") .arg(pAcquisition->CaseNumber    )))
      CHK (pAcquisition->Info.AddRow ("   " + t_DlgAcquire::tr("EwfEvidenceNumber") + QString(":: %1") .arg(pAcquisition->EvidenceNumber)))
      CHK (pAcquisition->Info.AddRow ("   " + t_DlgAcquire::tr("EwfExaminer"      ) + QString(":: %1") .arg(pAcquisition->Examiner      )))
      CHK (pAcquisition->Info.AddRow ("   " + t_DlgAcquire::tr("EwfDescription"   ) + QString(":: %1") .arg(pAcquisition->Description   )))
      CHK (pAcquisition->Info.AddRow ("   " + t_DlgAcquire::tr("EwfNotes"         ) + QString(":: %1") .arg(pAcquisition->Notes         )))

   }
   CHK (pAcquisition->Info.AddRow (tr("Image path and file name:: %1", "Info file").arg(pAcquisition->ImagePath + pAcquisition->ImageFilename + FormatExtension)))
   CHK (pAcquisition->Info.AddRow (tr("Info  path and file name:: %1", "Info file").arg(pAcquisition->InfoPath  + pAcquisition->InfoFilename  + t_File::pExtensionInfo)))

   QString HashText = t_InfoField::GetHashText (pAcquisition->CalcMD5, pAcquisition->CalcSHA1, pAcquisition->CalcSHA256);
   CHK (pAcquisition->Info.AddRow (tr("Hash calculation:: %1", "Info file") .arg (HashText)))

   if (pAcquisition->VerifySrc)
        CHK (pAcquisition->Info.AddRow (tr("Source verification:: on" , "Info file")))
   else CHK (pAcquisition->Info.AddRow (tr("Source verification:: off", "Info file")))

   if (pAcquisition->VerifyDst)
        CHK (pAcquisition->Info.AddRow (tr("Image verification:: on" , "Info file")))
   else CHK (pAcquisition->Info.AddRow (tr("Image verification:: off", "Info file")))

   CHK (pAcquisition->Info.WriteTable ());

   return NO_ERROR;
}

APIRET TableFifoCalcSizeAndAlloc (t_pDevice pDevice, unsigned int Fifos, unsigned int *pMaxBlocksInUse)
{
   const unsigned int PageSize = getpagesize();
   unsigned int       MaxBlocksInUse;
   unsigned int       ExtraSpace;

   // Set the data block size
   // -----------------------
   switch (pDevice->Acquisition1.Format)
   {
      case t_File::DD  : pDevice->FifoBlockSize = CONFIG(FifoBlockSizeDD );   break;
      case t_File::AAFF: pDevice->FifoBlockSize = CONFIG(FifoBlockSizeAFF);   break;
      case t_File::EWF:
         if (pDevice->HasCompressionThreads() && (CONFIG(EwfFormat) != t_File::AEWF))
              pDevice->FifoBlockSize = EWF_MULTITHREADED_COMPRESSION_CHUNK_SIZE;
         else pDevice->FifoBlockSize = CONFIG(FifoBlockSizeEWF);
         if (pDevice->FifoBlockSize != (unsigned int) CONFIG(FifoBlockSizeEWF))
            LOG_INFO ("[%s] Running with FifoBlockSize of %d (no other size supported when using libewf with multithreaded compression)", QSTR_TO_PSZ (pDevice->LinuxDevice), pDevice->FifoBlockSize)
         break;
      default:  CHK (ERROR_TABLE_INVALID_FORMAT)
   }

   // Calc the real amount of memory used for every block
   // ---------------------------------------------------
   pDevice->FifoAllocBlockSize = pDevice->FifoBlockSize;    // Start with the standard data size

   CHK (t_ThreadCompress::GetFifoExtraSpace (pDevice, &ExtraSpace))
   pDevice->FifoAllocBlockSize += ExtraSpace;

   pDevice->FifoAllocBlockSize += sizeof (t_FifoBlock);     // Add the structure overhead

//   if (!CONFIG (FifoMemoryManager))                       // Go to the next boundary when working with C-Lib memory management
//   {
      if ((pDevice->FifoAllocBlockSize % PageSize) != 0)
         pDevice->FifoAllocBlockSize += PageSize - (pDevice->FifoAllocBlockSize % PageSize);
//   }

   // Calc the number of entries for each FIFO
   // ----------------------------------------
   pDevice->FifoMaxBlocks = (int)((((long long)CONFIG(FifoMaxMem))*BYTES_PER_MEGABYTE) / ((long long)pDevice->FifoAllocBlockSize * Fifos));
   if (pDevice->FifoMaxBlocks == 0)
      pDevice->FifoMaxBlocks = 1;

   // Calc the amount of memory for the blocks and allocate it
   // --------------------------------------------------------
   MaxBlocksInUse = pDevice->FifoMaxBlocks * Fifos;  // Data in the FIFOs
   if (pDevice->Duplicate)
        MaxBlocksInUse += 2;                         // ThreadRead might allocate an additional block that
   else MaxBlocksInUse += 1;                         // can't yet be written to the full FIFO(s)
   if ((pDevice->Acquisition1.Format == t_File::AAFF) && !pDevice->HasCompressionThreads())
      MaxBlocksInUse += 1;                           // ThreadWrite might use an additional buffer if it does the AFF preprocessing
   if (pDevice->HasCompressionThreads())
      MaxBlocksInUse += CONFIG(CompressionThreads);  // Each compression thread might have one additional block for writing the compressed data
   MaxBlocksInUse += Fifos;
   *pMaxBlocksInUse = MaxBlocksInUse;

   if (CONFIG (FifoMemoryManager))
      CHK (FifoMemoryAlloc (&pDevice->pFifoMemory, MaxBlocksInUse, pDevice->FifoAllocBlockSize))

   return NO_ERROR;
}

APIRET t_Table::ShowAcquisitionDialog (t_pDevice pDevice, bool Clone)
{
   t_Acquisition Acquisition1(pDevice,1);
   t_Acquisition Acquisition2(pDevice,2);
   QString       Format;
   int           DlgRc;
   bool          Duplicate=false;
   bool          ForceLastEnteredValues=false;  // This one only is false for the 1st DlgAcquire that is shown. For all others it is true. Thus, the
                                                // 1st dialog is filled with the default values (as indicated in the configuration file). All other dialogs
   do                                           // (duplicate dialog, dialogs shown when switching forth and back between) show the values entered previously.
   {
      {
         t_DlgAcquire Dlg (pDevice, Clone, pOwn->pDeviceList, false, this, 0, ForceLastEnteredValues);
         DlgRc = Dlg.exec();
         ForceLastEnteredValues = true;
         switch (DlgRc)
         {
            case t_DlgAcquire::ButtonDuplicate:
               CHK (Dlg.GetParameters (Acquisition1))
               {
                  t_DlgAcquire DlgDuplicate (pDevice, Clone, pOwn->pDeviceList, true, this, 0, ForceLastEnteredValues, &Acquisition1);
                  DlgRc = DlgDuplicate.exec();
                  switch (DlgRc)
                  {
                     case t_DlgAcquire::ButtonOk:
                        CHK (DlgDuplicate.GetParameters (Acquisition2))
                        Duplicate = true;
                        break;
                     case t_DlgAcquire::ButtonBack:
                        break;
                     case t_DlgAcquire::ButtonCancel:
                        return NO_ERROR;
                     default:
                        return ERROR_TABLE_INVALID_DIALOG_CODE;
                  }
               }
               break;
            case t_DlgAcquire::ButtonOk:
               CHK (Dlg.GetParameters (Acquisition1))
               break;
            case t_DlgAcquire::ButtonCancel:
               return NO_ERROR;
            default:
               return ERROR_TABLE_INVALID_DIALOG_CODE;
         }
      }
   } while (DlgRc != t_DlgAcquire::ButtonOk);

   pDevice->Duplicate = Duplicate;
   pDevice->Acquisition1.SetDialogValues (Acquisition1);
   pDevice->Acquisition2.SetDialogValues (Acquisition2);
   pDevice->Error.Clear();
   pDevice->SetState (t_Device::Queued);

   pOwn->JobQueue.enqueue (pDevice);
   LOG_INFO ("[%s] Acquisition job queued, %d acquisitions waiting in queue now", QSTR_TO_PSZ(pDevice->LinuxDevice), pOwn->JobQueue.count());
   RescheduleJobs();

   return NO_ERROR;
}

APIRET t_Table::RescheduleJobs(void)
{
   t_pDevice pDevice;
   int        ActiveAcquisitions = 0;
   bool       LaunchNextJob      = true;

   LOG_INFO ("Rescheduling acquisition jobs");
   if (pOwn->JobQueue.isEmpty())
   {
      LOG_INFO ("No acquisition job waiting in queue");
   }
   else
   {
      if (CONFIG (LimitJobs) != CONFIG_LIMITJOBS_OFF)
      {
         for (int i=0; i<pOwn->pDeviceList->count(); i++)
         {
            if (pOwn->pDeviceList->at(i)->HasActiveAcquistion())
               ActiveAcquisitions++;
         }
         LaunchNextJob = (ActiveAcquisitions < CONFIG (LimitJobs));
      }

      if (LaunchNextJob)
      {
         pDevice = pOwn->JobQueue.dequeue();
         LOG_INFO ("[%s] Launching acquisition, %d more acquisitions in queue", QSTR_TO_PSZ(pDevice->LinuxDevice), pOwn->JobQueue.count());
         CHK (StartAcquisition (pDevice))
      }
   }

   return NO_ERROR;
}


APIRET t_Table::StartAcquisition (t_pDevice pDevice)
{
   QString Format;
   int     Fifos = 0;
   APIRET  rc;
   bool    Clone;

   Clone = pDevice->Acquisition1.Clone;
   pOwn->SlowDownAcquisitions = true;
   LOG_INFO ("[%s] Starting acquisition (%s) (%llu bytes)", QSTR_TO_PSZ (pDevice->LinuxDevice), Clone ? "clone":"image", pDevice->Size)
   CHK (t_File::GetFormatExtension  (pDevice->Acquisition1.Format, pDevice->Acquisition1.Clone, 0, nullptr, &Format))
   if (Clone)
      Format = " (clone)";
   LOG_INFO ("[%s] Image %s%s%s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ(pDevice->Acquisition1.ImagePath), QSTR_TO_PSZ(pDevice->Acquisition1.ImageFilename), QSTR_TO_PSZ(Format))
   LOG_INFO ("[%s] Info  %s%s%s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ(pDevice->Acquisition1.InfoPath ), QSTR_TO_PSZ(pDevice->Acquisition1.InfoFilename ), t_File::pExtensionInfo)
   if (pDevice->Duplicate)
   {
      LOG_INFO ("[%s] Image is duplicated", QSTR_TO_PSZ (pDevice->LinuxDevice))
      LOG_INFO ("[%s] Image %s%s%s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ(pDevice->Acquisition2.ImagePath), QSTR_TO_PSZ(pDevice->Acquisition2.ImageFilename), QSTR_TO_PSZ(Format))
      LOG_INFO ("[%s] Info  %s%s%s", QSTR_TO_PSZ (pDevice->LinuxDevice), QSTR_TO_PSZ(pDevice->Acquisition2.InfoPath ), QSTR_TO_PSZ(pDevice->Acquisition2.InfoFilename ), t_File::pExtensionInfo)
   }
   CHK (pDevice->SetMessage (QString()))
   pDevice->SetState (t_Device::Launch);  // Starting with state "Launch", switching to "Acquire" as soon as a Rescan (triggered at end of this fn) has been done and device is still the same (see t_MainWindow::SlotScanFinished)
   pDevice->Error.Clear();
   pDevice->AbortCount          = 0;
   pDevice->DeleteAfterAbort    = false;
   pDevice->StartTimestampVerify= QDateTime();
   pDevice->StopTimestamp       = QDateTime();
   pDevice->PrevTimestamp       = QTime();
   pDevice->PrevSpeed           = 0.0;
   pDevice->PrevPos             = 0;
   pDevice->Acquisition1.CurrentWritePos.Zero();
   pDevice->Acquisition2.CurrentWritePos.Zero();
   pDevice->CurrentVerifyPosSrc.Zero();
   pDevice->Acquisition1.CurrentVerifyPosDst.Zero();
   pDevice->Acquisition2.CurrentVerifyPosDst.Zero();
   CHK (pDevice->ClearBadSectors ())

   memset (&pDevice->MD5Digest            , 0, sizeof(pDevice->MD5Digest         ));
   memset (&pDevice->MD5DigestVerifySrc   , 0, sizeof(pDevice->MD5DigestVerifySrc));
   memset (&pDevice->Acquisition1.MD5DigestVerifyDst, 0, sizeof(pDevice->Acquisition1.MD5DigestVerifyDst));
   memset (&pDevice->Acquisition2.MD5DigestVerifyDst, 0, sizeof(pDevice->Acquisition2.MD5DigestVerifyDst));

   memset (&pDevice->SHA1Digest         , 0, sizeof(pDevice->SHA1Digest         ));
   memset (&pDevice->SHA1DigestVerifySrc, 0, sizeof(pDevice->SHA1DigestVerifySrc));
   memset (&pDevice->Acquisition1.SHA1DigestVerifyDst, 0, sizeof(pDevice->Acquisition1.SHA1DigestVerifyDst));
   memset (&pDevice->Acquisition2.SHA1DigestVerifyDst, 0, sizeof(pDevice->Acquisition2.SHA1DigestVerifyDst));

   memset (&pDevice->SHA256Digest         , 0, sizeof(pDevice->SHA256Digest         ));
   memset (&pDevice->SHA256DigestVerifySrc, 0, sizeof(pDevice->SHA256DigestVerifySrc));
   memset (&pDevice->Acquisition1.SHA256DigestVerifyDst, 0, sizeof(pDevice->Acquisition1.SHA256DigestVerifyDst));
   memset (&pDevice->Acquisition2.SHA256DigestVerifyDst, 0, sizeof(pDevice->Acquisition2.SHA256DigestVerifyDst));
   pDevice->Acquisition1.ImageFileHashList.clear();
   pDevice->Acquisition2.ImageFileHashList.clear();
   pDevice->StartTimestamp = QDateTime::currentDateTime();
   rc = InfoAcquisitionStart (&pDevice->Acquisition1);
   if (rc != NO_ERROR)
   {
      if ((rc == ERROR_INFO_FILE_CREATE_FAILED) || (rc == ERROR_INFO_FILE_OPEN_FAILED  ) ||
          (rc == ERROR_INFO_FILE_WRITE_FAILED ) || (rc == ERROR_INFO_FILE_CLOSE_FAILED ))
           pDevice->Error.Set (t_Device::t_Error::InfoError, 1);
      else pDevice->Error.Set (t_Device::t_Error::AcquisitionStartFailed);
   }
   if (!pDevice->Error.Abort() && pDevice->Duplicate)
   {
      rc = InfoAcquisitionStart (&pDevice->Acquisition2);
      if (rc != NO_ERROR)
      {
         if ((rc == ERROR_INFO_FILE_CREATE_FAILED) || (rc == ERROR_INFO_FILE_OPEN_FAILED  ) ||
             (rc == ERROR_INFO_FILE_WRITE_FAILED ) || (rc == ERROR_INFO_FILE_CLOSE_FAILED ))
              pDevice->Error.Set (t_Device::t_Error::InfoError, 2);
         else pDevice->Error.Set (t_Device::t_Error::AcquisitionStartFailed);
      }
   }
   if (pDevice->Error.Abort())
      return rc;

   if (pDevice->pThreadRead   != nullptr) CHK_EXIT (ERROR_TABLE_THREADREAD_ALREADY_RUNNING )
   if (pDevice->pThreadWrite1 != nullptr) CHK_EXIT (ERROR_TABLE_THREADWRITE_ALREADY_RUNNING)
   if (pDevice->pThreadWrite2 != nullptr) CHK_EXIT (ERROR_TABLE_THREADWRITE_ALREADY_RUNNING)
   if (pDevice->pThreadHash   != nullptr) CHK_EXIT (ERROR_TABLE_THREADHASH_ALREADY_RUNNING )
   if (!pDevice->ThreadCompressList.isEmpty()) CHK_EXIT (ERROR_TABLE_THREADCOMPRESS_ALREADY_RUNNING)
   if ((pDevice->pFifoRead        != nullptr) ||
       (pDevice->pFifoHashIn      != nullptr) ||
       (pDevice->pFifoHashOut     != nullptr) ||
       (pDevice->pFifoCompressIn  != nullptr) ||
       (pDevice->pFifoCompressOut != nullptr))
      CHK_EXIT (ERROR_TABLE_FIFO_EXISTS)

   // Create threads
   // --------------
   pDevice->pThreadRead   = new t_ThreadRead  ( pDevice              , &pOwn->SlowDownAcquisitions);
   pDevice->pThreadWrite1 = new t_ThreadWrite (&pDevice->Acquisition1, &pDevice->pFifoWrite1, &pOwn->SlowDownAcquisitions);
   if (pDevice->Duplicate)
      pDevice->pThreadWrite2 = new t_ThreadWrite (&pDevice->Acquisition2, &pDevice->pFifoWrite2, &pOwn->SlowDownAcquisitions);
   CHK_QT_EXIT (connect (pDevice->pThreadRead  , SIGNAL(SignalEnded       (t_pDevice)), this, SLOT(SlotThreadReadFinished    (t_pDevice))))
   CHK_QT_EXIT (connect (pDevice->pThreadWrite1, SIGNAL(SignalEnded       (t_pDevice)), this, SLOT(SlotThreadWrite1Finished   (t_pDevice))))
   CHK_QT_EXIT (connect (pDevice->pThreadWrite1, SIGNAL(SignalFreeMyHandle(t_pDevice)), this, SLOT(SlotThreadWriteWakeThreads (t_pDevice))))
   if (pDevice->Duplicate)
   {
      CHK_QT_EXIT (connect (pDevice->pThreadWrite2, SIGNAL(SignalEnded       (t_pDevice)), this, SLOT(SlotThreadWrite2Finished   (t_pDevice))))
      CHK_QT_EXIT (connect (pDevice->pThreadWrite2, SIGNAL(SignalFreeMyHandle(t_pDevice)), this, SLOT(SlotInvalidFreeHandle      (t_pDevice))))
   }

   if (pDevice->HasHashThread())
   {
      pDevice->pThreadHash = new t_ThreadHash (pDevice);
      CHK_QT_EXIT (connect (pDevice->pThreadHash, SIGNAL(SignalEnded(t_pDevice)), this, SLOT(SlotThreadHashFinished (t_pDevice))))
   }

   if (pDevice->HasCompressionThreads())
   {
      for (int i=0; i < CONFIG(CompressionThreads); i++)
      {
         t_pThreadCompress pThread;

         pThread = new t_ThreadCompress (pDevice, i);
         pDevice->ThreadCompressList.append (pThread);
         CHK_QT_EXIT (connect (pThread, SIGNAL(SignalEnded(t_pDevice,int)), this, SLOT(SlotThreadCompressFinished (t_pDevice,int))))
      }
   }

   // Create fifos and assign threads
   // -------------------------------
   unsigned int MaxBlocksInUse;

   if      (!pDevice->HasHashThread() && !pDevice->HasCompressionThreads())
   {
      if (pDevice->Duplicate)
           Fifos = 2;
      else Fifos = 1;
      CHK (TableFifoCalcSizeAndAlloc (pDevice, Fifos, &MaxBlocksInUse))
      pDevice->pFifoRead   = new t_FifoStd (pDevice->pFifoMemory, pDevice->FifoMaxBlocks);
      pDevice->pFifoWrite1 = pDevice->pFifoRead;
   }
   else if ( pDevice->HasHashThread() && !pDevice->HasCompressionThreads())
   {
      if (pDevice->Duplicate)
           Fifos = 3;
      else Fifos = 2;
      CHK (TableFifoCalcSizeAndAlloc (pDevice, Fifos, &MaxBlocksInUse))
      pDevice->pFifoRead    = new t_FifoStd (pDevice->pFifoMemory, pDevice->FifoMaxBlocks);
      pDevice->pFifoHashIn  = pDevice->pFifoRead;
      pDevice->pFifoHashOut = new t_FifoStd (pDevice->pFifoMemory, pDevice->FifoMaxBlocks);
      pDevice->pFifoWrite1  = pDevice->pFifoHashOut;
   }
   else if (!pDevice->HasHashThread() &&  pDevice->HasCompressionThreads())
   {
      if (pDevice->Duplicate)
           Fifos = 3 * CONFIG(CompressionThreads);
      else Fifos = 2 * CONFIG(CompressionThreads);
      CHK (TableFifoCalcSizeAndAlloc (pDevice, Fifos, &MaxBlocksInUse))
      pDevice->pFifoCompressIn  = new t_FifoCompressIn  (pDevice->pFifoMemory, CONFIG(CompressionThreads), pDevice->FifoMaxBlocks);
      pDevice->pFifoRead        = pDevice->pFifoCompressIn;
      pDevice->pFifoCompressOut = new t_FifoCompressOut (pDevice->pFifoMemory, CONFIG(CompressionThreads), pDevice->FifoMaxBlocks);
      pDevice->pFifoWrite1      = pDevice->pFifoCompressOut;
   }
   else if ( pDevice->HasHashThread() &&  pDevice->HasCompressionThreads())
   {
      if (pDevice->Duplicate)
           Fifos = 1 + 3 * CONFIG(CompressionThreads);
      else Fifos = 1 + 2 * CONFIG(CompressionThreads);
      CHK (TableFifoCalcSizeAndAlloc (pDevice, Fifos, &MaxBlocksInUse))
      pDevice->pFifoRead        = new t_FifoStd (pDevice->pFifoMemory, pDevice->FifoMaxBlocks);
      pDevice->pFifoHashIn      = pDevice->pFifoRead;
      pDevice->pFifoCompressIn  = new t_FifoCompressIn  (pDevice->pFifoMemory, CONFIG(CompressionThreads), pDevice->FifoMaxBlocks);
      pDevice->pFifoHashOut     = pDevice->pFifoCompressIn;
      pDevice->pFifoCompressOut = new t_FifoCompressOut (pDevice->pFifoMemory, CONFIG(CompressionThreads), pDevice->FifoMaxBlocks);
      pDevice->pFifoWrite1      = pDevice->pFifoCompressOut;
   }
   if (pDevice->Duplicate)
      CHK (pDevice->pFifoWrite1->DupFifo(&pDevice->pFifoWrite2));

   LOG_INFO ("[%s] Acquisition runs with %d fifos, each one with space for %d blocks of %d bytes (%0.1f MB FIFO space)",
             QSTR_TO_PSZ (pDevice->LinuxDevice),
             Fifos, pDevice->FifoMaxBlocks, pDevice->FifoBlockSize,
             ((double)Fifos * pDevice->FifoMaxBlocks * pDevice->FifoBlockSize) / BYTES_PER_MEGABYTE)
   LOG_INFO ("[%s] Including the overhead, %u blocks of %d bytes have been allocated (%0.1f MB)",
             QSTR_TO_PSZ (pDevice->LinuxDevice),
             MaxBlocksInUse, pDevice->FifoAllocBlockSize,
             ((double)MaxBlocksInUse * pDevice->FifoAllocBlockSize) / BYTES_PER_MEGABYTE)

   // Start threads
   // -------------
   pDevice->pThreadRead ->start(QThread::LowPriority);
   if (pDevice->HasHashThread())
      pDevice->pThreadHash->start(QThread::LowPriority);
   for (int i=0; i < pDevice->ThreadCompressList.count(); i++)
      pDevice->ThreadCompressList[i]->start(QThread::LowPriority);
   pDevice->pThreadWrite1->start(QThread::LowPriority);
   if (pDevice->Duplicate)
      pDevice->pThreadWrite2->start(QThread::LowPriority);

   pOwn->SlowDownAcquisitions = false;

   pOwn->pMainWindow->SlotRescan(); // Rescan in order be sure that launched acquisition still refers to the correct device

   return NO_ERROR;
}


// ----------------------------------------------------------------------------------------------------------------
//
//                                                     Acquistion end
//
// ----------------------------------------------------------------------------------------------------------------

APIRET t_Table::InfoAcquisitionBadSectors (t_pAcquisition pAcquisition, bool Verify)
{
   t_pDevice      pDevice = pAcquisition->pDevice;
   QList<quint64>  BadSectors;
   quint64         Count, i;
   quint64         From, To, Next;
   int             LineEntries    =  0;
   const int       MaxLineEntries = 10;
   bool            First          = true;

   CHK (pDevice->GetBadSectors (BadSectors, Verify))
   Count = BadSectors.count();
   if (Count)
   {
      if (Verify)
           LOG_INFO ("[%s] During verification, %lld bad sectors have been encountered", QSTR_TO_PSZ (pDevice->LinuxDevice), Count)
      else LOG_INFO ("[%s] During acquisition, %lld bad sectors have been encountered", QSTR_TO_PSZ (pDevice->LinuxDevice), Count)

      if (Verify)
           CHK (pAcquisition->Info.WriteLn (tr("During verification, %1 bad sectors have been encountered. The sector numbers are:", "Info file") .arg(Count)))
      else CHK (pAcquisition->Info.WriteLn (tr("During acquisition, %1 bad sectors have been encountered. They have been replaced by zeroed sectors. The sector numbers are:", "Info file") .arg(Count)))
      CHK (pAcquisition->Info.WriteLn ("   "))
      i = 0;
      while (i<Count)
      {
         From = BadSectors.at(i++);
         To   = From;
         while (i < Count)
         {
            Next = BadSectors.at(i);
            if (Next != To+1)
               break;
            To = Next;
            i++;
         }
         if (!First)
         {
            if (LineEntries >= MaxLineEntries)
            {
               CHK (pAcquisition->Info.Write   (","))
               CHK (pAcquisition->Info.WriteLn ("   "))
               LineEntries = 0;
            }
            else
            {
               CHK (pAcquisition->Info.Write (", "))
            }
         }
         First = false;
         if (From == To)
         {
            CHK (pAcquisition->Info.Write ("%llu", From))
            LineEntries++;
         }
         else
         {
            CHK (pAcquisition->Info.Write ("%llu-%llu", From, To))
            LineEntries += 2;
         }
      }
   }
   else
   {
      if (Verify)
      {
         LOG_INFO ("[%s] No bad sectors encountered during verification %d.", QSTR_TO_PSZ (pDevice->LinuxDevice), pAcquisition->Instance)
         CHK (pAcquisition->Info.WriteLn (tr("No bad sectors encountered during verification.", "Info file")))
      }
      else
      {
         LOG_INFO ("[%s] No bad sectors encountered during acquisition %d.", QSTR_TO_PSZ (pDevice->LinuxDevice), pAcquisition->Instance)
         CHK (pAcquisition->Info.WriteLn (tr("No bad sectors encountered during acquisition.", "Info file")))
      }
   }

   return NO_ERROR;
}

static APIRET TableTimestampToISO (const QDateTime &Timestamp, QString &Str)
{
   Str = Timestamp.toString (Qt::ISODate);
   Str.replace ("T", " ");
   return NO_ERROR;
}

APIRET t_Table::InfoAcquisitionEnd0 (t_pAcquisition pAcquisition)
{
   t_pDevice        pDevice = pAcquisition->pDevice;
   QString           None;
   QString           MD5;
   QString           MD5VerifySrc;
   QString           MD5VerifyDst;
   QString           SHA1;
   QString           SHA1VerifySrc;
   QString           SHA1VerifyDst;
   QString           SHA256;
   QString           SHA256VerifySrc;
   QString           SHA256VerifyDst;
   QString           StateStr;
   quint64           BadSectors;
   QString           StartStr, StopStr, VerifyStr;
   QString           Filename;
   int               Hours, Minutes, Seconds;
   double            Speed;
   bool              MatchSrc = true;
   bool              MatchDst = true;
   t_Device::t_State DeviceState;
   uint              InstanceError;

   CHK (pAcquisition->Info.WriteLn ())
   CHK (InfoAcquisitionBadSectors (pAcquisition, false))
   if (pAcquisition->VerifySrc)
      CHK (InfoAcquisitionBadSectors (pAcquisition, true))
   None= tr("--", "Info file");
   MD5          = None;     SHA1          = None;   SHA256          = None;
   MD5VerifySrc = None;     SHA1VerifySrc = None;   SHA256VerifySrc = None;
   MD5VerifyDst = None;     SHA1VerifyDst = None;   SHA256VerifyDst = None;

   DeviceState   = pDevice->GetState();                                                       // In case of duplicate acquisitions, this function is called for
   InstanceError = pDevice->Error.Get(pAcquisition->Instance);                                // each acquisition separately in order to procuce separate info files.
   if ( (DeviceState == t_Device::Aborted ) ||                                                // We therefore not only must respect the general acquisition state, but
       ((DeviceState == t_Device::Finished) && (InstanceError != t_Device::t_Error::None)))   // also the individual instances.
   {
      switch (pDevice->Error.Get())
      {
         case t_Device::t_Error::UserRequest:
            StateStr = tr("Aborted by user", "Info file") + " ";
            if (pDevice->StartTimestampVerify.isNull())
            {
               StateStr += tr("(during acquisition)"     , "Info file");
            }
            else
            {
               StateStr += tr("(during source verification)", "Info file");
               if (pAcquisition->CalcMD5   ) CHK (HashMD5DigestStr    (&pDevice->MD5Digest   , MD5   )) // If the user interrupted during
               if (pAcquisition->CalcSHA1  ) CHK (HashSHA1DigestStr   (&pDevice->SHA1Digest  , SHA1  )) // verification (i.e. after acquisition),
               if (pAcquisition->CalcSHA256) CHK (HashSHA256DigestStr (&pDevice->SHA256Digest, SHA256)) // we can calculate the acquisition hash.
            }
            break;
         case t_Device::t_Error::AcquisitionStartFailed:
            StateStr = tr("Aborted because acquisition failed to start", "Info file");
            break;
         case t_Device::t_Error::ReadError:
            StateStr = tr("Aborted because of source read error", "Info file");
            break;
         case t_Device::t_Error::TooManyBadSectors:
            StateStr = tr("Aborted, too many bad sectors (> %1)", "Info file") .arg(CONFIG(JobMaxBadSectors));
            break;
         case t_Device::t_Error::DisconnectTimeout:
            StateStr = tr("Aborted because device has been inaccessible for too long (> %1s)", "Info file") .arg (CONFIG(JobDisconnectTimeout));
            break;
         default:
            switch (InstanceError)
            {
               case t_Device::t_Error::VerifyError:
                  StateStr = tr("Aborted because of image read error during verification", "Info file");
                  if (pAcquisition->CalcMD5   ) CHK (HashMD5DigestStr    (&pDevice->MD5Digest   , MD5   ))
                  if (pAcquisition->CalcSHA1  ) CHK (HashSHA1DigestStr   (&pDevice->SHA1Digest  , SHA1  ))
                  if (pAcquisition->CalcSHA256) CHK (HashSHA256DigestStr (&pDevice->SHA256Digest, SHA256))
                  break;
               case t_Device::t_Error::WriteError : StateStr = tr("Aborted because of image write error"       , "Info file"); break;
               case t_Device::t_Error::InfoError  : StateStr = tr("Aborted because of info file write error"   , "Info file"); break;
               default:                             StateStr = tr("Aborted, strange reason (%1 %2)"            , "Info file") .arg(pDevice->Error.Get()) .arg(pDevice->Error.Get(pAcquisition->Instance)); break;
            }
      }
   }
   else if (DeviceState == t_Device::Finished)
   {
      StateStr = tr("Finished successfully", "Info file");
      BadSectors = t_Device::GetBadSectorCount(pDevice).toULongLong();
      if (BadSectors)
         StateStr += " " + tr("(with %1 bad sectors)", "Info file, may be appended to 'finished successfully' message") .arg(BadSectors);
      if (pAcquisition->CalcMD5   ) CHK (HashMD5DigestStr    (&pDevice->MD5Digest   , MD5   ))
      if (pAcquisition->CalcSHA1  ) CHK (HashSHA1DigestStr   (&pDevice->SHA1Digest  , SHA1  ))
      if (pAcquisition->CalcSHA256) CHK (HashSHA256DigestStr (&pDevice->SHA256Digest, SHA256))
      if (pAcquisition->VerifySrc)
      {
         if (pAcquisition->CalcMD5   ) CHK (HashMD5DigestStr    (&pDevice->MD5DigestVerifySrc   , MD5VerifySrc   ))
         if (pAcquisition->CalcSHA1  ) CHK (HashSHA1DigestStr   (&pDevice->SHA1DigestVerifySrc  , SHA1VerifySrc  ))
         if (pAcquisition->CalcSHA256) CHK (HashSHA256DigestStr (&pDevice->SHA256DigestVerifySrc, SHA256VerifySrc))
      }
      if (pAcquisition->VerifyDst)
      {
         if (pAcquisition->CalcMD5   ) CHK (HashMD5DigestStr    (&pAcquisition->MD5DigestVerifyDst   , MD5VerifyDst   ))
         if (pAcquisition->CalcSHA1  ) CHK (HashSHA1DigestStr   (&pAcquisition->SHA1DigestVerifyDst  , SHA1VerifyDst  ))
         if (pAcquisition->CalcSHA256) CHK (HashSHA256DigestStr (&pAcquisition->SHA256DigestVerifyDst, SHA256VerifyDst))
      }
   }
   else
   {
      StateStr = tr("Strange state (%1)", "Info file") .arg(pDevice->GetState());
   }

   CHK (pAcquisition->Info.WriteLn (tr("State: %1", "Info file") .arg(StateStr)))
   CHK (pAcquisition->Info.WriteLn ())

   CHK (pAcquisition->Info.AddRow (tr("MD5 hash:: %1"                   , "Info file") .arg(MD5            )))
   CHK (pAcquisition->Info.AddRow (tr("MD5 hash verified source:: %1"   , "Info file") .arg(MD5VerifySrc   )))
   CHK (pAcquisition->Info.AddRow (tr("MD5 hash verified image:: %1"    , "Info file") .arg(MD5VerifyDst   )))
   CHK (pAcquisition->Info.AddRow (tr("SHA1 hash:: %1"                  , "Info file") .arg(SHA1           )))
   CHK (pAcquisition->Info.AddRow (tr("SHA1 hash verified source:: %1"  , "Info file") .arg(SHA1VerifySrc  )))
   CHK (pAcquisition->Info.AddRow (tr("SHA1 hash verified image:: %1"   , "Info file") .arg(SHA1VerifyDst  )))
   CHK (pAcquisition->Info.AddRow (tr("SHA256 hash:: %1"                , "Info file") .arg(SHA256         )))
   CHK (pAcquisition->Info.AddRow (tr("SHA256 hash verified source:: %1", "Info file") .arg(SHA256VerifySrc)))
   CHK (pAcquisition->Info.AddRow (tr("SHA256 hash verified image:: %1" , "Info file") .arg(SHA256VerifyDst)))
   CHK (pAcquisition->Info.WriteTable ());

   if ((pDevice->GetState() == t_Device::Finished) && pAcquisition->VerifySrc)
   {
      if (pAcquisition->CalcMD5)    MatchSrc =            HashMD5Match   (&pDevice->MD5Digest   , &pDevice->MD5DigestVerifySrc   );
      if (pAcquisition->CalcSHA1)   MatchSrc = MatchSrc | HashSHA1Match  (&pDevice->SHA1Digest  , &pDevice->SHA1DigestVerifySrc  );
      if (pAcquisition->CalcSHA256) MatchSrc = MatchSrc | HashSHA256Match(&pDevice->SHA256Digest, &pDevice->SHA256DigestVerifySrc);

      if (MatchSrc) CHK (pAcquisition->Info.WriteLn (tr ("Source verification OK. The device delivered the same data during acquisition and verification.")))
      else          CHK (pAcquisition->Info.WriteLn (tr ("Source verification FAILED. The device didn't deliver the same data during acquisition and verification. "
                                                    "Check if the defect sector list was the same during acquisition and verification (see above).")))
   }
   if ((pDevice->GetState() == t_Device::Finished) && pAcquisition->VerifyDst)
   {
      if (pAcquisition->CalcMD5)    MatchDst =            HashMD5Match   (&pDevice->MD5Digest   , &pAcquisition->MD5DigestVerifyDst   );
      if (pAcquisition->CalcSHA1)   MatchDst = MatchDst | HashSHA1Match  (&pDevice->SHA1Digest  , &pAcquisition->SHA1DigestVerifyDst  );
      if (pAcquisition->CalcSHA256) MatchDst = MatchDst | HashSHA256Match(&pDevice->SHA256Digest, &pAcquisition->SHA256DigestVerifyDst);

      if (MatchDst) CHK (pAcquisition->Info.WriteLn (tr ("Image verification OK. The image contains exactely the data that was written.")))
      else          CHK (pAcquisition->Info.WriteLn (tr ("Image verification FAILED. The data in the image is different from what was written.")))
   }
   if (!MatchSrc || !MatchDst)
      CHK (pAcquisition->Info.WriteLn (tr ("Maybe you try to acquire the device again.")))

   CHK (pAcquisition->Info.WriteLn ())

   // Performance
   // -----------
   CHK (TableTimestampToISO (pDevice->StartTimestamp, StartStr))
   CHK (TableTimestampToISO (pDevice->StopTimestamp , StopStr ))
   StartStr += " " + tr("(ISO format YYYY-MM-DD HH:MM:SS)");
   Seconds  = pDevice->StartTimestamp.secsTo (pDevice->StopTimestamp);
   Hours    = Seconds / SECONDS_PER_HOUR  ; Seconds -= Hours   * SECONDS_PER_HOUR;
   Minutes  = Seconds / SECONDS_PER_MINUTE; Seconds -= Minutes * SECONDS_PER_MINUTE;

   if (pAcquisition->VerifySrc || pAcquisition->VerifyDst)
   {
      if (!pDevice->StartTimestampVerify.isNull())
           CHK (TableTimestampToISO (pDevice->StartTimestampVerify , VerifyStr))
      else VerifyStr = tr ("Acquisition aborted before start of verification");

      CHK (pAcquisition->Info.AddRow (tr("Acquisition started:: %1" , "Info file") .arg(StartStr )))
      CHK (pAcquisition->Info.AddRow (tr("Verification started:: %1", "Info file") .arg(VerifyStr)))
      CHK (pAcquisition->Info.AddRow (tr("Ended:: %1 (%2 hours, %3 minutes and %4 seconds)", "Info file").arg(StopStr ) .arg(Hours) .arg(Minutes) .arg(Seconds)))

      if (pDevice->StartTimestampVerify.isNull())
           Seconds = GETMAX (pDevice->StartTimestamp.secsTo (pDevice->StopTimestamp)       , 1);
      else Seconds = GETMAX (pDevice->StartTimestamp.secsTo (pDevice->StartTimestampVerify), 1);
      Speed = (double) pDevice->CurrentReadPos.Get() / ((double)BYTES_PER_MEGABYTE * (double)Seconds);
      Hours   = Seconds / SECONDS_PER_HOUR  ; Seconds -= Hours   * SECONDS_PER_HOUR;
      Minutes = Seconds / SECONDS_PER_MINUTE; Seconds -= Minutes * SECONDS_PER_MINUTE;
      CHK (pAcquisition->Info.AddRow (tr("Acquisition speed:: %1 MByte/s (%2 hours, %3 minutes and %4 seconds)", "Info file").arg(Speed, 0, 'f', 2) .arg(Hours) .arg(Minutes) .arg(Seconds)))

      if (!pDevice->StartTimestampVerify.isNull())
      {
         Seconds = GETMAX (pDevice->StartTimestampVerify.secsTo (pDevice->StopTimestamp), 1);
         Speed = (double) pDevice->CurrentReadPos.Get() / ((double)BYTES_PER_MEGABYTE * (double)Seconds);
         Hours   = Seconds / SECONDS_PER_HOUR  ; Seconds -= Hours   * SECONDS_PER_HOUR;
         Minutes = Seconds / SECONDS_PER_MINUTE; Seconds -= Minutes * SECONDS_PER_MINUTE;
         CHK (pAcquisition->Info.AddRow (tr("Verification speed:: %1 MByte/s (%2 hours, %3 minutes and %4 seconds)", "Info file").arg(Speed, 0, 'f', 2) .arg(Hours) .arg(Minutes) .arg(Seconds)))
      }
      CHK (pAcquisition->Info.WriteTable ());
   }
   else
   {
      CHK (pAcquisition->Info.AddRow (tr("Acquisition started:: %1"                        , "Info file").arg(StartStr)))
      CHK (pAcquisition->Info.AddRow (tr("Ended:: %1 (%2 hours, %3 minutes and %4 seconds)", "Info file").arg(StopStr ) .arg(Hours) .arg(Minutes) .arg(Seconds)))

      Seconds = GETMAX (pDevice->StartTimestamp.secsTo (pDevice->StopTimestamp), 1);
      Speed = (double) pDevice->CurrentReadPos.Get() / ((double)BYTES_PER_MEGABYTE * (double)Seconds);
      Hours   = Seconds / SECONDS_PER_HOUR  ; Seconds -= Hours   * SECONDS_PER_HOUR;
      Minutes = Seconds / SECONDS_PER_MINUTE; Seconds -= Minutes * SECONDS_PER_MINUTE;
      CHK (pAcquisition->Info.AddRow (tr("Acquisition speed:: %1 MByte/s (%2 hours, %3 minutes and %4 seconds)", "Info file").arg(Speed, 0, 'f', 2) .arg(Hours) .arg(Minutes) .arg(Seconds)))
      CHK (pAcquisition->Info.WriteTable ());
   }

   // Images files and hashes
   // -----------------------
   t_ImageFileHash *pFileHash;

   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.WriteLn ())
   CHK (pAcquisition->Info.Title   (tr("Generated image files and their MD5 hashes", "Info file")))
   CHK (pAcquisition->Info.WriteLn ())
   if (CONFIG(CalcImageFileMD5))
   {
      if (pAcquisition->VerifyDst)
      {
         if ((pAcquisition->Format == t_File::EWF) && (CONFIG(EwfFormat) != t_File::AEWF))
            CHK (pAcquisition->Info.WriteLn (tr("No MD5 hashes available (the selected EWF format does not support image file hashes)")))
      }
      else
      {
         CHK (pAcquisition->Info.WriteLn (tr("No MD5 hashes available (image verification checkbox was switched off)")))
      }
   }
   else
   {
      CHK (pAcquisition->Info.WriteLn (tr("No MD5 hashes available (configuration parameter CalcImageFileMD5 is off)")))
   }
   CHK (pAcquisition->Info.WriteLn ("MD5                               Image file"))
   for (int i=0; i<pAcquisition->ImageFileHashList.count(); i++)
   {
      pFileHash = pAcquisition->ImageFileHashList[i];
      if (pFileHash->MD5Valid)
           CHK (HashMD5DigestStr(&pFileHash->MD5Digest, MD5))
      else MD5 = "n/a                             ";
      Filename = QFileInfo(pFileHash->Filename).fileName();
      CHK (pAcquisition->Info.WriteLn (QString("%1  %2") .arg(MD5) .arg(Filename)))
   }

   if (pDevice->GetState() == t_Device::Aborted)
   {
      CHK (pAcquisition->Info.WriteLn ());
      CHK (pAcquisition->Info.WriteLn (tr("Acquisition/verification aborted - file list may be incomplete")))
   }
   CHK (pAcquisition->Info.WriteLn ());

   return NO_ERROR;
}

APIRET t_Table::InfoAcquisitionEnd (t_pAcquisition pAcquisition)
{
   APIRET rc;

   rc = InfoAcquisitionEnd0 (pAcquisition);
   if (rc != NO_ERROR)
   {
      if ((rc == ERROR_INFO_FILE_CREATE_FAILED) || (rc == ERROR_INFO_FILE_OPEN_FAILED  ) ||
          (rc == ERROR_INFO_FILE_WRITE_FAILED ) || (rc == ERROR_INFO_FILE_CLOSE_FAILED ))
           pAcquisition->pDevice->Error.Set(t_Device::t_Error::InfoError, pAcquisition->Instance);
      else CHK_EXIT (rc)

      if (pAcquisition->pDevice->Error.Abort())
         pAcquisition->pDevice->SetState (t_Device::Aborted);
   }
   return NO_ERROR;
}

static APIRET TableCallAcquisitionEndCommand (t_pDevice pDevice)
{
   QString Command;
   APIRET  rc;

   Command = CONFIG (CommandAcquisitionEnd);

   if (!Command.isEmpty())
   {
      QString Extension;
      QString StateStr;

      CHK (t_File::GetFormatExtension (pDevice, nullptr, &Extension))

      switch (pDevice->GetState())
      {
         case t_Device::Aborted:  StateStr = "Aborted";  break;
         case t_Device::Finished: StateStr = "Finished"; break;
         default:                 StateStr = QString ("Unexpected state %1") .arg(pDevice->GetState());
      }
      t_Device::ProcessTokenString (Command, pDevice);

      rc = QtUtilProcessCommand (Command);
      if (rc == ERROR_QTUTIL_COMMAND_TIMEOUT)
      {
         LOG_INFO ("Timeout while trying to call acquisition end command for %s.", QSTR_TO_PSZ(pDevice->LinuxDevice))
         return NO_ERROR;
      }
      CHK(rc)
   }
   return NO_ERROR;
}

APIRET t_Table::FinaliseThreadStructs (t_pDevice pDevice)
{
   if ((pDevice->pThreadRead   == nullptr) &&        // t_Table::FinaliseThreadStructs is called upon every thread's end,
       (pDevice->pThreadWrite1 == nullptr) &&        // but its core only is executed after the last thread ended (as
       (pDevice->pThreadWrite2 == nullptr) &&        // we must not kill vital structures as long as any threads are
       (pDevice->pThreadHash   == nullptr) &&        // still running).
       (pDevice->ThreadCompressList.isEmpty()))
   {
      pDevice->StopTimestamp = QDateTime::currentDateTime();

      if      (!pDevice->HasHashThread() && !pDevice->HasCompressionThreads())
      {
         delete pDevice->pFifoRead;
      }
      else if ( pDevice->HasHashThread() && !pDevice->HasCompressionThreads())
      {
         delete pDevice->pFifoRead;
         delete pDevice->pFifoHashOut;
      }
      else if (!pDevice->HasHashThread() &&  pDevice->HasCompressionThreads())
      {
         delete pDevice->pFifoCompressIn;
         delete pDevice->pFifoCompressOut;
      }
      else if ( pDevice->HasHashThread() &&  pDevice->HasCompressionThreads())
      {
         delete pDevice->pFifoRead;
         delete pDevice->pFifoCompressIn;
         delete pDevice->pFifoCompressOut;
      }

      pDevice->pFifoRead        = nullptr;
      pDevice->pFifoHashIn      = nullptr;
      pDevice->pFifoHashOut     = nullptr;
      pDevice->pFifoWrite1      = nullptr;
      pDevice->pFifoWrite2      = nullptr;
      pDevice->pFifoCompressIn  = nullptr;
      pDevice->pFifoCompressOut = nullptr;

      if (CONFIG (FifoMemoryManager))
         CHK (FifoMemoryFree (&pDevice->pFifoMemory))

//      LibEwfGetMemStats (&LibEwfAllocs, &LibEwfFrees);
//      LOG_INFO ("LIBEWF mem  statistics: %d allocated - %d freed = %d remaining", LibEwfAllocs, LibEwfFrees, LibEwfAllocs - LibEwfFrees)

      if (pDevice->Error.Abort())
           pDevice->SetState (t_Device::Aborted );
      else pDevice->SetState (t_Device::Finished);

      if (!pDevice->DeleteAfterAbort)
      {
         CHK_EXIT (InfoAcquisitionEnd (&pDevice->Acquisition1))
         if (pDevice->Duplicate)
            CHK_EXIT (InfoAcquisitionEnd (&pDevice->Acquisition2))
      }

      while (!pDevice->Acquisition1.ImageFileHashList.isEmpty())
         delete pDevice->Acquisition1.ImageFileHashList.takeFirst();

      while (!pDevice->Acquisition2.ImageFileHashList.isEmpty())
         delete pDevice->Acquisition2.ImageFileHashList.takeFirst();

      LOG_INFO ("[%s] All structures cleaned.", QSTR_TO_PSZ (pDevice->LinuxDevice))

      // Call user's acquisition end command
      // -----------------------------------
      CHK (TableCallAcquisitionEndCommand (pDevice))

      // Start other acquisitions wating in queue
      // ----------------------------------------
      CHK (RescheduleJobs())

      // Check if all acquisitions have completed now (for AutoExit)
      // -----------------------------------------------------------
      t_Device::t_State State;
      int     i;
      bool    AllCompleted = true;

      for (i=0; i<pOwn->pDeviceList->count() && AllCompleted; i++)
      {
         State = pOwn->pDeviceList->at(i)->GetState();
         AllCompleted =  (State == t_Device::Idle    ) ||
                         (State == t_Device::Finished) ||
                        ((State == t_Device::Aborted ) && (pOwn->pDeviceList->at(i)->Error.Get() == t_Device::t_Error::UserRequest));
      }
      if (AllCompleted)
         emit (SignalAllAcquisitionsEnded());
   }

   return NO_ERROR;
}

void t_Table::SlotThreadReadFinished (t_pDevice pDevice)
{
   LOG_INFO ("[%s] Read thread finished", QSTR_TO_PSZ (pDevice->LinuxDevice))
   pDevice->pThreadRead->deleteLater();
   pDevice->pThreadRead = nullptr;

   if (pDevice->Error.Abort())
        CHK_EXIT (pDevice->WakeWaitingThreads())
   else CHK_EXIT (pDevice->pFifoRead->InsertDummy ())
   CHK_EXIT (FinaliseThreadStructs (pDevice))
}

void t_Table::SlotThreadHashFinished (t_pDevice pDevice)
{
   LOG_INFO ("[%s] Hash thread finished", QSTR_TO_PSZ (pDevice->LinuxDevice))
   pDevice->pThreadHash->deleteLater();
   pDevice->pThreadHash = nullptr;

   if (pDevice->Error.Abort())
        CHK_EXIT (pDevice->WakeWaitingThreads())
   else CHK_EXIT (pDevice->pFifoHashOut->InsertDummy ())
   CHK_EXIT (FinaliseThreadStructs (pDevice))
}

void t_Table::SlotThreadCompressFinished (t_pDevice pDevice, int ThreadNr)
{
   bool NoMoreThreads = true;

   LOG_INFO ("[%s] Compression thread #%d finished", QSTR_TO_PSZ (pDevice->LinuxDevice), ThreadNr)

   pDevice->ThreadCompressList[ThreadNr]->deleteLater();
   pDevice->ThreadCompressList[ThreadNr] = nullptr;

   for (int i=0;
        (i < pDevice->ThreadCompressList.count()) && NoMoreThreads;
        i++)
      NoMoreThreads = (pDevice->ThreadCompressList[i] == nullptr);

   if (NoMoreThreads)
   {
       LOG_INFO ("[%s] All %d compression threads finished", QSTR_TO_PSZ (pDevice->LinuxDevice), pDevice->ThreadCompressList.count())
       pDevice->ThreadCompressList.clear();
   }

   if (pDevice->Error.Abort())
        CHK_EXIT (pDevice->WakeWaitingThreads())
   else CHK_EXIT (pDevice->pFifoCompressOut->InsertDummy (ThreadNr))

   CHK_EXIT (FinaliseThreadStructs (pDevice))
}

APIRET t_Table::ThreadWriteFinished (t_pDevice pDevice, t_pThreadWrite *ppThreadWrite)
{
   if (pDevice->Duplicate)
        LOG_INFO ("[%s] Write thread finished"   , QSTR_TO_PSZ (pDevice->LinuxDevice))
   else LOG_INFO ("[%s] Write thread %d finished", QSTR_TO_PSZ (pDevice->LinuxDevice), (*ppThreadWrite)->Instance())

   (*ppThreadWrite)->deleteLater();
   *ppThreadWrite = nullptr;

   if (pDevice->Error.Abort())
      CHK_EXIT (pDevice->WakeWaitingThreads())

   CHK_EXIT (FinaliseThreadStructs (pDevice))

   return NO_ERROR;
}

void t_Table::SlotThreadWrite1Finished (t_pDevice pDevice)
{
   CHK_EXIT (ThreadWriteFinished (pDevice, &pDevice->pThreadWrite1))
}

void t_Table::SlotThreadWrite2Finished (t_pDevice pDevice)
{
   CHK_EXIT (ThreadWriteFinished (pDevice, &pDevice->pThreadWrite2))
}

void t_Table::SlotThreadWriteWakeThreads (t_pDevice pDevice)
{
   LOG_INFO ("[%s] Write thread asks other threads for releasing its handle", QSTR_TO_PSZ (pDevice->LinuxDevice))

   if (pDevice->Error.Abort())
   {
      LOG_INFO ("[%s] Calling WakeWaitingThreads", QSTR_TO_PSZ (pDevice->LinuxDevice))
      CHK_EXIT (pDevice->WakeWaitingThreads())
   }
}

void t_Table::SlotInvalidFreeHandle (t_pDevice pDevice)
{
   LOG_ERROR ("[%s] Write thread 2 asks other threads for releasing its handle - which should never happen!", QSTR_TO_PSZ (pDevice->LinuxDevice))
   CHK_EXIT (ERROR_TABLE_INVALID_FREE_HANDLE)
}

APIRET t_Table::AbortAcquisition0 (t_pDevice pDevice)
{
   if (pDevice->Error.Abort())
   {
      LOG_INFO ("[%s] User pressed abort, but abort flag is already set.", QSTR_TO_PSZ(pDevice->LinuxDevice))
      return NO_ERROR;
   }
   LOG_INFO ("[%s] User aborts acquisition", QSTR_TO_PSZ(pDevice->LinuxDevice))
   pDevice->Error.Set (t_Device::t_Error::UserRequest);

   if (pOwn->JobQueue.removeOne(pDevice))
        pDevice->SetState (t_Device::Aborted);  // Acquisition was queued, so we only need to change the state
   else CHK (pDevice->WakeWaitingThreads ())    // Aquisition already was running if not in queue

   return NO_ERROR;
}

APIRET t_Table::AbortAcquisition (t_pDevice pDevice)
{
   bool Abort;

   if (pDevice->Error.Abort())
        Abort = true;
   else CHK (t_DlgAbort::Show (pDevice, Abort, pDevice->DeleteAfterAbort))

   if (Abort)
   {
      pDevice->AbortCount++;
      CHK (AbortAcquisition0 (pDevice))
   }
   CHK (RescheduleJobs())

   return NO_ERROR;
}

APIRET t_Table::AbortAllAcquisitions (void)
{
   t_pDevice pDevice;
   int        i;

   for (i=0; i<pOwn->pDeviceList->count(); i++)
   {
      pDevice = pOwn->pDeviceList->at(i);
      if ((pDevice->GetState() == t_Device::Launch)        ||
          (pDevice->GetState() == t_Device::LaunchPaused)  ||
          (pDevice->GetState() == t_Device::Acquire)       ||
          (pDevice->GetState() == t_Device::AcquirePaused) ||
          (pDevice->GetState() == t_Device::Verify)        ||
          (pDevice->GetState() == t_Device::VerifyPaused))
      {
         CHK (AbortAcquisition0 (pDevice))
      }
   }

   return NO_ERROR;
}

