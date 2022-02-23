// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Acquisition dialog
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
#include <QDesktopWidget>

#include "toolconstants.h"

#include "common.h"
#include "compileinfo.h"
#include "config.h"
#include "util.h"
#include "qtutil.h"
#include "dlgdirsel.h"
#include "devicelistmodel.h"
#include "dlgacquire.h"
#include "dlgacquire_private.h"
#include "dlgmessage.h"
#include "main.h"
#include "mainwindow.h"

// -----------------------------
//           Constants
// -----------------------------

static const char *DLGACQUIRE_DEFAULT_EMPTY_FILENAME   = "Out";  // Default image file name if - after removing special chars - the file name is empty.
static const char *DLGACQUIRE_PROPERTY_SENDER_LINEEDIT = "SenderLineEdit";

// -----------------------------
//           Classes
// -----------------------------

class t_DlgAcquireLocal
{
   public:
      bool              Clone;
      bool              Duplicate;
      QRadioButton    *pRadioButtonFormatDD;
      QRadioButton    *pRadioButtonFormatEWF;
      QRadioButton    *pRadioButtonFormatAFF;

      QList<QWidget*>   EwfWidgetsList;      // Used to comfortably enable / disable EWF related entry fields and labels

      QTableWidget    *pDeviceTable;         // Only created and displayed when cloning a device

      QPushButton     *pButtonOk;
      QPushButton     *pButtonDuplicate;
      QPushButton     *pButtonBack;
      QPushButton     *pButtonCancel;

      t_pDevice        pDevice;
      t_pDeviceList    pDeviceList;
      t_pAcquisition   pAcquisition1;        // Contains the values entered in the first acquisition dialog in case this dialog is the duplicate dialog. Used
                                             // for checking consistency of values entered in both acquisition dialogs (cross check info and image filenames)

      bool              OldSplitFileSwitch;  // Required to remember SplitFileSwitch when chaging forth and back
                                             // between formats that support split files and not.
};


class t_DlgAcquireTableWidgetItem : public QTableWidgetItem    // t_DlgAcquireTableWidgetItem contains a pointer to the coreesponding
{                                                              // device in order to be able to find out the device when clicking on a
   public:                                                     // line in the clone dialog device table.
      t_DlgAcquireTableWidgetItem (t_pcDevice pDevice, const QString &Text) :
         QTableWidgetItem (Text),
         poDevice (pDevice)
      {
      }

      t_pcDevice GetpDevice (void)
      {
         return poDevice;
      }

   private:
      t_pcDevice poDevice;
};

static t_File::Format DlgAcquireLastUsedFormat = t_File::NotSet;  // For remembering the format that was used for the last acquisition

// -----------------------------
//         Unit conversion
// -----------------------------

const QStringList UnitStrings = QStringList() << "MiB" << "GiB" << "TiB" << "PiB" << "EiB";

static int UnitStringToIndex (QString &Str)
{
   if      (Str.contains ('M', Qt::CaseInsensitive)) return 0;
   else if (Str.contains ('G', Qt::CaseInsensitive)) return 1;
   else if (Str.contains ('T', Qt::CaseInsensitive)) return 2;
   else if (Str.contains ('P', Qt::CaseInsensitive)) return 3;
   else if (Str.contains ('E', Qt::CaseInsensitive)) return 4;
   LOG_ERROR ("Unknown Unit %s", QSTR_TO_PSZ(Str))
   return 0;
}

static unsigned long long UnitIndexToMultiplier (int Index)
{
   unsigned long long Mul = 1024*1024;

   if ((Index < 0) || (Index > 4))
        LOG_ERROR ("Unit index out of range: %d", Index)
   else Mul <<= (Index*10);

   return Mul;
}

// -----------------------------
//    Field utility functions
// -----------------------------

static t_pCfgDlgAcquireField DlgAcquireGetField (const QString &Name)
{
   t_pCfgDlgAcquireFields pDlgAcquireFields;
   t_pCfgDlgAcquireField  pDlgAcquireField;
   int i;

   CHK_EXIT (CfgGetDlgAcquireFields (&pDlgAcquireFields))

   for (i=0; i<pDlgAcquireFields->count(); i++)
   {
      pDlgAcquireField = pDlgAcquireFields->at(i);
      if (pDlgAcquireField->FieldName.compare(Name, Qt::CaseInsensitive)==0)
         return pDlgAcquireField;
   }
   LOG_ERROR ("DlgAcquire field %s not found", QSTR_TO_PSZ(Name))
   return nullptr;
}


static t_pDlgAcquireLineEdit DlgAcquireGetLineEdit (const QString &Name)
{
   return DlgAcquireGetField(Name)->pLineEdit;
}


static APIRET DlgAcquireResolveSpecialSequences (t_pDevice pDevice, bool ConsiderFields, const QString &In, QString &Out)
{
   t_pCfgDlgAcquireFields pDlgAcquireFields;
   t_pCfgDlgAcquireField  pDlgAcquireField;
   QDateTime               Now = QDateTime::currentDateTime();
   int                     i;
   QString                 Search;

   // Replace tokens
   // --------------
   Out = In;
   t_Device::ProcessTokenString (Out, pDevice);

   // Replace field values
   // --------------------
   if (ConsiderFields)
   {
      CHK_EXIT (CfgGetDlgAcquireFields (&pDlgAcquireFields))
      for (i=0; i<pDlgAcquireFields->count(); i++)
      {
         pDlgAcquireField = pDlgAcquireFields->at(i);
         if (pDlgAcquireField->pLineEdit)
         {
            Search = "%" + pDlgAcquireField->FieldName + "%";
            Out.replace (Search, pDlgAcquireField->pLineEdit->text());
         }
      }
   }

   return NO_ERROR;
}

static APIRET DlgAcquireGetFieldValue (t_pDevice pDevice, bool Clone, t_pCfgDlgAcquireField pDlgAcquireField, bool ForceLastEnteredValues, QString &Set)
{
   t_CfgEntryMode EntryMode;

   if (Clone && ((pDlgAcquireField->FieldName == CFG_DLGACQUIRE_DEST_IMAGEDIRECTORY) ||
                 (pDlgAcquireField->FieldName == CFG_DLGACQUIRE_DEST_IMAGEFILENAME )))
   {
      Set = QString();
      return NO_ERROR;
   }

   if (ForceLastEnteredValues)
   {
      EntryMode = CFG_ENTRYMODE_SHOWLAST;
   }
   else
   {
      if (Clone)
           EntryMode = pDlgAcquireField->EntryModeClone;
      else EntryMode = pDlgAcquireField->EntryModeImage;
   }

   switch (EntryMode)
   {
      case CFG_ENTRYMODE_HIDE:
      case CFG_ENTRYMODE_SHOWLAST:
         Set = pDlgAcquireField->LastEnteredValue;
         if (Set.isNull())
         {
            Set = pDlgAcquireField->DefaultValue;
            CHK (DlgAcquireResolveSpecialSequences (pDevice, false, pDlgAcquireField->DefaultValue, Set))
         }
         break;
      case CFG_ENTRYMODE_SHOWDEFAULT:
         Set = pDlgAcquireField->DefaultValue;
         CHK (DlgAcquireResolveSpecialSequences (pDevice, false, pDlgAcquireField->DefaultValue, Set))
         break;
      default:
         CHK (ERROR_DLGACQUIRE_INVALID_ENTRYMODE)
   }
   return NO_ERROR;
}

// -----------------------------
//         t_DlgAcquire
// -----------------------------

static bool DlgAcquireStrToBool (const QString &Set)
{
   return !Set.isEmpty() && (Set.compare ("NO"         , Qt::CaseInsensitive) != 0)  &&
          !Set.isNull () && (Set.compare ("OFF"        , Qt::CaseInsensitive) != 0)  &&
                            (Set.compare ("FALSE"      , Qt::CaseInsensitive) != 0)  &&
                            (Set.compare ("0"          , Qt::CaseInsensitive) != 0)  &&
                            (Set.compare ("DISABLED"   , Qt::CaseInsensitive) != 0)  &&
                            (Set.compare ("DEACTIVATED", Qt::CaseInsensitive) != 0);
}

t_CfgEntryMode t_DlgAcquire::EntryMode (t_pCfgDlgAcquireField pField)
{
   return pOwn->Clone ? pField->EntryModeClone : pField->EntryModeImage;
}

APIRET t_DlgAcquire::AddField (t_pDevice pDevice, t_pCfgDlgAcquireField pField, QGridLayout *pLayout, int *pRow, int *pCol, bool ForceLastEnteredValues)
{
   t_pDlgAcquireLineEdit  pLineEdit     = nullptr;
   QCheckBox             *pCheckBox     = nullptr;
   QComboBox             *pComboBox     = nullptr;
   QLabel                *pLabel        = nullptr;
   QWidget               *pEntryWidget  = nullptr;
   QToolButton           *pButtonBrowse = nullptr;
   QString                 Set;
   bool                    Ok;

   // Create field
   // ------------
   if (pField->HashField || pField->FieldName == CFG_DLGACQUIRE_SPLITFILESWITCH)
   {
      pEntryWidget = pCheckBox = new QCheckBox (tr(QSTR_TO_PSZ(pField->FieldName)), this);
   }
   else if (pField->FieldName == CFG_DLGACQUIRE_SPLITFILEUNIT)
   {
      pEntryWidget = pComboBox = new QComboBox (this);
      pComboBox->addItems (UnitStrings);
   }
   else
   {
      pEntryWidget = pLineEdit = new t_DlgAcquireLineEdit (this, pField->FieldName);
      pLabel       = new QLabel (tr(QSTR_TO_PSZ(pField->FieldName)), this);
   }

   if ((pField->DirField) && (EntryMode(pField) != CFG_ENTRYMODE_HIDE))
   {
      pButtonBrowse = new QToolButton (this);
      QSize MaximumSize = pButtonBrowse->maximumSize();
      MaximumSize.setWidth (USHRT_MAX);
      pButtonBrowse->setMaximumSize (MaximumSize);
      pButtonBrowse->setText (tr("...", "The directory browse button"));
   }
   // Position field
   // --------------
   pField->pLineEdit = pLineEdit;
   pField->pCheckBox = pCheckBox;
   pField->pComboBox = pComboBox;
   pField->pLabel    = pLabel;
   if (EntryMode(pField) == CFG_ENTRYMODE_HIDE)
   {
      if (pLabel)
         pLabel->hide();
      pEntryWidget->hide();
   }
   else
   {
      if (pField->FieldName == CFG_DLGACQUIRE_SPLITFILESIZE)
      {
         pLayout->addWidget (pLabel   , *pRow, *pCol  );
         pLayout->addWidget (pLineEdit, *pRow, *pCol+1);
      }
      else
      {
         if (pButtonBrowse)
         {
            pLayout->addWidget (pLabel       , *pRow, 0);
            pLayout->addWidget (pButtonBrowse, *pRow, 1);
         }
         else if (pLabel)
         {
            pLayout->addWidget (pLabel, *pRow, 0, 1, 2);        // if there's no button then use the first 2 columns for the label
         }
         if (pLineEdit)
         {
            pLayout->addWidget (pLineEdit, *pRow, 2);
            if (pField->EwfField)
            {
               pOwn->EwfWidgetsList.append (pLabel   );
               pOwn->EwfWidgetsList.append (pLineEdit);
            }
            (*pRow)++;
         }
         if (pCheckBox)
         {
            if (pField->FieldName == CFG_DLGACQUIRE_SPLITFILESWITCH)
            {
               pLayout->addWidget (pCheckBox, *pRow, *pCol, 1, 2);
               (*pRow)++;
            }
            else if (pField->FieldName.startsWith (CFG_DLGACQUIRE_HASHCALC_FIELDID))
            {
               pLayout->addWidget (pCheckBox, 0, *pCol);  // Put all HashCalc field in the first row
               (*pCol)++;
            }
            else
            {
               pLayout->addWidget (pCheckBox, (*pRow)+1, 0, 1, 2); // Put each verification field in an own row below
               (*pRow)++;
            }
         }
         if (pComboBox)
         {
            pLayout->addWidget (pComboBox, *pRow, 3);
         }
      }
   }

   // Set field value
   // ---------------
   CHK_EXIT (DlgAcquireGetFieldValue (pDevice, pOwn->Clone, pField, ForceLastEnteredValues, Set))
   if (pField->DirField)
   {
      if (!CONFIG(DirectoryFieldEditing))
         pLineEdit->setReadOnly (true);
      if (!Set.endsWith ("/"))
         Set += "/";
   }
   if (pLineEdit)
   {
      pLineEdit->setText (Set);
      CHK_QT_EXIT (connect (pLineEdit, SIGNAL (SignalTextEdited (t_DlgAcquireLineEdit *, const QString &)),
                                 this, SLOT   (SlotTextEdited   (t_DlgAcquireLineEdit *, const QString &))))
   }

   if (pField->DstField)
      CHK_QT_EXIT (connect (pLineEdit, SIGNAL (textChanged(const QString &)), this, SLOT(UpdateDialogState(const QString &))))

   if (pCheckBox)
      pCheckBox->setChecked (DlgAcquireStrToBool(Set));

   if (pComboBox)
   {
      pComboBox->setCurrentIndex(Set.toInt(&Ok));
      if (!Ok)
         LOG_ERROR ("Invalid set value for ComboBox: %s", QSTR_TO_PSZ(Set));
   }
   if (pButtonBrowse)
   {
      (void) pButtonBrowse->setProperty (DLGACQUIRE_PROPERTY_SENDER_LINEEDIT, qVariantFromValue ((void *)pLineEdit));
      CHK_QT_EXIT (connect (pButtonBrowse, SIGNAL (released()), this, SLOT(SlotBrowse())))
   }

   return NO_ERROR;
}

static APIRET DlgAcquireGetCol (int i, t_pcCfgColumn *ppColCfg, t_pMainWindowColumn *ppColDef)
{
   *ppColCfg = CfgGetColumn (i);
   if ((*ppColCfg)->ShowInCloneTable)
   {
      *ppColDef = MainWindowGetColumn ((*ppColCfg)->Name);
      if (*ppColDef == nullptr)
         CHK(ERROR_DLGACQUIRE_UNKNOWN_COLUMN)
   }
   else
   {
      *ppColCfg = nullptr;
      *ppColDef = nullptr;
   }

   return NO_ERROR;
}

APIRET t_DlgAcquire::InsertDeviceTableRow (QTableWidget *pTable, int Row, t_pDeviceList pDeviceList, t_pDevice pDevSrc, t_pDevice pDevDst)
{
   t_DlgAcquireTableWidgetItem *pItem;
   t_pcCfgColumn                pColCfg;
   t_pMainWindowColumn          pColDef;
   QString                       Remark;
   QString                       Str;
   bool                          Selectable = false;
   int                           Cols;
   int                           Col;
   int                           i;

   Cols = CfgGetColumnCount();

   // Build header row on 1st call
   // ----------------------------
   if (Row == 0)
   {
      for (i=0, Col=0; i<Cols; i++)
         if (CfgGetColumn(i)->ShowInCloneTable)
            Col++;
      pTable->setColumnCount (Col+1); // +1 for column "remark"

      for (i=0, Col=0; i<Cols; i++)
      {
         CHK (DlgAcquireGetCol (i, &pColCfg, &pColDef))
         if (pColCfg)
            pTable->setHorizontalHeaderItem (Col++, new QTableWidgetItem(t_DeviceListModel::tr(pColCfg->Name)));
      }
      pTable->setHorizontalHeaderItem (Col, new QTableWidgetItem(t_DeviceListModel::tr("Remark")));
   }

   // Insert row data
   // ---------------
   bool InUse = (pDevDst->GetState() != t_Device::Idle    ) &&
                (pDevDst->GetState() != t_Device::Finished) &&
                (pDevDst->GetState() != t_Device::Aborted );

   if      (pDevDst->Local)                               Remark = tr("Local device, cannot be written");
   else if (pDevDst       == pDevSrc)                     Remark = tr("Device to be cloned");
   else if (pDevDst->Size <  pDevSrc->Size)               Remark = tr("Too small");
   else if (InUse)                                        Remark = tr("In use");
   else if (pOwn->Duplicate && ((pOwn->pAcquisition1->ImagePath +
                                 pOwn->pAcquisition1->ImageFilename) == pDevDst->LinuxDevice))
                                                          Remark = tr("Selected for first acquisition");
   else if (pDeviceList->UsedAsCloneDestination(pDevDst)) Remark = tr("Used in another clone operation");
   else                                                 { Remark = tr("Ok for cloning"); Selectable = true;}

   #define ADD_ITEM(Str)                                                                            \
   {                                                                                                \
      pItem = new t_DlgAcquireTableWidgetItem (pDevDst, Str);                                       \
      pTable->setItem (Row, Col++, pItem);                                                          \
      pItem->setFlags (Selectable ? (Qt::ItemIsSelectable | Qt::ItemIsEnabled) : Qt::NoItemFlags);  \
   }

   for (i=0, Col=0; i<Cols; i++)
   {
      CHK (DlgAcquireGetCol (i, &pColCfg, &pColDef))
      if (pColCfg)
         ADD_ITEM ((pColDef->pGetDataFn)(pDevDst).toString())
   }
   ADD_ITEM (Remark);

   return NO_ERROR;
}


APIRET t_DlgAcquire::CreateDeviceTable (t_pDeviceList pDeviceList, t_pDevice pDeviceSrc, QTableWidget **ppTable)
{
   QTableWidget *pTable;
   t_pDevice     pDev;
   int            i=0;
   int            ColWidth=0;
   int            ExtraWidth;
   int            ScreenWidth;
   int            Width=0;

   pTable = new QTableWidget(0, 0, this);
   *ppTable = pTable;
   pOwn->pDeviceTable = pTable;

   QTUTIL_SET_FONT (pTable, FONTOBJECT_TABLE)
   pTable->setSelectionBehavior (QAbstractItemView::SelectRows     );
   pTable->setSelectionMode     (QAbstractItemView::SingleSelection);

   pTable->setRowCount(pDeviceList->count());
   for (i=0; i<pDeviceList->count(); i++)
   {
      pDev = pDeviceList->at (i);
      CHK (InsertDeviceTableRow (pTable, i, pDeviceList, pDeviceSrc, pDev))
   }
   ExtraWidth = pTable->columnWidth(0);  // Somehow, the table widget always gets this value larger when calling setMinimumWidth...
   pTable->resizeColumnsToContents();
   pTable->resizeRowsToContents   ();
   pTable->verticalHeader  ()->hide ();
//   pTable->horizontalHeader()->setClickable          (true);
//   pTable->horizontalHeader()->setSortIndicatorShown (true);
   pTable->setHorizontalScrollMode (QAbstractItemView::ScrollPerPixel);
   pTable->setVerticalScrollMode   (QAbstractItemView::ScrollPerPixel);

   for (i=0; i<pTable->columnCount(); i++)
      ColWidth += pTable->columnWidth(i);

   ScreenWidth = QApplication::desktop()->availableGeometry().width();
   Width = GETMIN ((int)(0.8 * ScreenWidth), ColWidth - ExtraWidth);   // Do not use more than 80% of the screen width
   pTable->setMinimumWidth  (Width);
   pTable->setMinimumHeight (130);

   CHK_QT (connect (pTable, SIGNAL(itemSelectionChanged ()), this, SLOT(SlotDeviceTableSelectionChanged())))

   return NO_ERROR;
}

t_DlgAcquire::t_DlgAcquire ()
{
   CHK_EXIT (ERROR_DLGACQUIRE_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised


class t_DlgAcquireLayoutScroller: public QScrollArea // This class makes layouts scrollable.
{
   public:
      t_DlgAcquireLayoutScroller (QWidget *pParent, QLayout *pLayout)
         : QScrollArea (pParent)
      {
         setFrameStyle      (QFrame::NoFrame);  // We do not want any frames
         setLineWidth       (0);                // nor margins
         setMidLineWidth    (0);
         setContentsMargins (0,0,0,0);
         setWidgetResizable (true);      // necessary to have the layout inside the ScrollArea follow the dialog's resizing

         QWidget   *pWidget = new QWidget (this);  // we need an intermediate widget, as QScrollArea only accepts
         setWidget (pWidget);                      // a widget and does ont allow for setting a layout directly
         pWidget->setLayout (pLayout);             // We then put the layout in the widget

         setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff); // Only scroll vertically
      }

     ~t_DlgAcquireLayoutScroller ()
      {
      }

      virtual QSize sizeHint () const
      {
         QWidget *pWidget;
         QLayout *pLayout;

         pWidget = widget();
         if (pWidget)
         {
            pLayout = pWidget->layout();
            if (pLayout)                       // Give the layout's minimal size as size hint. So, when putting the whole
               return pLayout->minimumSize (); // scroll area in a layout, it should be displayed just big enough to have
         }                                     // the scroll bar hidden.
         return QScrollArea::sizeHint();
      }

      virtual QSize minimumSizeHint () const                      // In hor. direction, the area must never be smaller then
      {                                                           // the minimum required by the layout inside (as we do not
         return QSize (sizeHint().width(),                        // want a hor. scrollbar and thus switched it off, see
                       QScrollArea::minimumSizeHint().height());  // constructor above). Vertically, the default minimum
      }                                                           // of QScrollArea is used (whatever this might be).
};


// t_DlgAcquire::t_DlgAcquire: Duplicate is false for the 1st acquisition dialog and true for the 2nd one.
t_DlgAcquire::t_DlgAcquire (t_pDevice pDevice, bool Clone, t_pDeviceList pDeviceList, bool Duplicate, QWidget *pParent, Qt::WindowFlags Flags, bool ForceLastEnteredValues, t_pAcquisition pAcquisition1)
   :QDialog (pParent, Flags)
{
   static bool                  Initialised = false;
   t_pCfgDlgAcquireFields      pDlgAcquireFields;
   t_pCfgDlgAcquireField       pDlgAcquireField;
   t_DlgAcquireLayoutScroller *pLayoutScroller;
   QVBoxLayout                *pTopLayout;
   QVBoxLayout                *pLayout;
   QLabel                     *pLabel;
   QString                      DefaultFilename;
   QString                      Path;
   QString                      Str, StrAlt;
   QString                      ButtonTextDD;
   QString                      ButtonTextEWF;
   QString                      ButtonTextAFF;
   QString                      Set;
   int                          Row, Col;
   int                          i;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGACQUIRE_CONSTRUCTOR_NOT_SUPPORTED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGACQUIRE_UNKNOWN_FILEDIALOG_SIZE))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGACQUIRE_INVALID_ENTRYMODE))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGACQUIRE_INVALID_FORMAT))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGACQUIRE_INVALID_SELECTION))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGACQUIRE_UNKNOWN_COLUMN))

      pDlgAcquireField = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILEUNIT);                                 // This initialisation needs to be done only once, as the user writes text (MiB, GiB, ...)
      pDlgAcquireField->DefaultValue = QString::number (UnitStringToIndex(pDlgAcquireField->DefaultValue)); // in the configuration file, but we internally work with the ComboBox index numbers
   }

   QTUTIL_SET_FONT (this, FONTOBJECT_ACQUISITION_DIALOGS)

   if      (!Duplicate && !Clone)     setWindowTitle (tr ("Acquire image of %1"  , "Dialog title, %1 is the device (for instance /dev/hdc)") .arg(pDevice->LinuxDevice));
   else if (!Duplicate &&  Clone)     setWindowTitle (tr ("Clone %1"             , "Dialog title, %1 is the device (for instance /dev/hdc)") .arg(pDevice->LinuxDevice));
   else if ( Duplicate && !Clone)     setWindowTitle (tr ("Duplicate image of %1", "Dialog title, %1 is the device (for instance /dev/hdc)") .arg(pDevice->LinuxDevice));
   else  /*( Duplicate &&  Clone) */  setWindowTitle (tr ("Duplicate Clone %1"   , "Dialog title, %1 is the device (for instance /dev/hdc)") .arg(pDevice->LinuxDevice));

   pOwn    = new t_DlgAcquireLocal;
   pOwn->pDevice       = pDevice;
   pOwn->Clone         = Clone;
   pOwn->pDeviceTable  = nullptr;
   pOwn->Duplicate     = Duplicate;
   pOwn->pAcquisition1 = pAcquisition1;
   pOwn->pDeviceList   = pDeviceList;

   // Organisation of the whole dialog
   //     Dialog
   //        TopLayout
   //           ScrollArea    - t_DlgAcquireLayoutScroller
   //              Widget     - see t_DlgAcquireLayoutScroller
   //                 Layout  - see t_DlgAcquireLayoutScroller
   //           LayoutButtons

   pTopLayout = new QVBoxLayout(this);
   pTopLayout->setSpacing(0);  // We do not want extra margins everywhere (see t_DlgAcquireScrollArea as well)
   pTopLayout->setMargin (0);

   pLayout = new QVBoxLayout();
   pLayout->setSizeConstraint (QLayout::SetMinAndMaxSize);
   pLayoutScroller = new t_DlgAcquireLayoutScroller (this, pLayout);
   pTopLayout->addWidget(pLayoutScroller);

   CHK_EXIT (CfgGetDlgAcquireFields (&pDlgAcquireFields))

   if (Duplicate)
      pLayout->addWidget (new QLabel(("<b><font color=\"blue\"><center>" + tr("Duplicate image settings") + "</center></font></b>"), this));

   if (!Clone)
   {
      // Format box (with EWF fields)
      // ----------------------------
      QGroupBox   *pGroupBoxFormat    = new QGroupBox (tr("File format"), this);
      QVBoxLayout *pLayoutGroupBox    = new QVBoxLayout;
      QHBoxLayout *pLayoutFormatSplit = new QHBoxLayout;
      QGridLayout *pLayoutFormat      = new QGridLayout;
      QGridLayout *pLayoutSplit       = new QGridLayout;
      QGridLayout *pLayoutEwf         = new QGridLayout;
      QFrame      *pSeparator         = new QFrame(this); // vertical separator line between format and split fields
      pSeparator->setFrameShape (QFrame::VLine);

      pLayout           ->addWidget (pGroupBoxFormat);
      pGroupBoxFormat   ->setLayout (pLayoutGroupBox);
      pLayoutGroupBox   ->addLayout (pLayoutFormatSplit);
      pLayoutFormatSplit->addLayout (pLayoutFormat);
      pLayoutFormatSplit->addWidget (pSeparator);
      pLayoutFormatSplit->addLayout (pLayoutSplit);
      pLayoutGroupBox   ->addLayout (pLayoutEwf);

      CHK_EXIT (t_File::GetFormatDescription (t_File::DD  , false, 0, Str))            ButtonTextDD  = "&" + Str;
      CHK_EXIT (t_File::GetFormatDescription (t_File::EWF , false, 0, Str))            ButtonTextEWF = "&" + Str;
      CHK_EXIT (t_File::GetFormatDescription (t_File::AAFF, false, 0, Str))            ButtonTextAFF = "&" + Str;
      CHK_EXIT (t_File::GetFormatExtension   (t_File::DD  , false, 0, nullptr, &Str))
      CHK_EXIT (t_File::GetFormatExtension   (t_File::DD  , false, 3, nullptr, &StrAlt))  ButtonTextDD += " " + tr("(file extension %1 or %2)") .arg (Str) .arg (StrAlt);
      CHK_EXIT (t_File::GetFormatExtension   (t_File::EWF , false, 0, nullptr, &Str))     ButtonTextEWF+= " " + tr("(file extension %1)") .arg (Str);
      CHK_EXIT (t_File::GetFormatExtension   (t_File::AAFF, false, 0, nullptr, &Str))     ButtonTextAFF+= " " + tr("(file extension %1)") .arg (Str);

      pOwn->pRadioButtonFormatDD  = new QRadioButton (ButtonTextDD , this);
      pOwn->pRadioButtonFormatEWF = new QRadioButton (ButtonTextEWF, this);
      pOwn->pRadioButtonFormatAFF = new QRadioButton (ButtonTextAFF, this);

      pLayoutFormat->addWidget  (pOwn->pRadioButtonFormatDD );
      pLayoutFormat->addWidget  (pOwn->pRadioButtonFormatEWF);
      if (CONFIG(AffEnabled))
      {
         pLayoutFormat->addWidget  (pOwn->pRadioButtonFormatAFF);
      }
      else
      {
         pOwn->pRadioButtonFormatAFF->hide();
         pOwn->pRadioButtonFormatAFF->setEnabled(false);
         pOwn->pRadioButtonFormatAFF->setChecked(false);
      }

      if (Duplicate)
      {
         pOwn->pRadioButtonFormatDD ->setEnabled(false);
         pOwn->pRadioButtonFormatEWF->setEnabled(false);
         pOwn->pRadioButtonFormatAFF->setEnabled(false);
      }

      // Split file controls
      // -------------------
      t_pCfgDlgAcquireField pFieldSplitFileSwitch = DlgAcquireGetField ("SplitFileSwitch");
      t_pCfgDlgAcquireField pFieldSplitFileSize   = DlgAcquireGetField ("SplitFileSize"  );
      t_pCfgDlgAcquireField pFieldSplitFileUnit   = DlgAcquireGetField ("SplitFileUnit"  );

      if ((EntryMode(pFieldSplitFileSwitch) == CFG_ENTRYMODE_HIDE) &&
          (EntryMode(pFieldSplitFileSize  ) == CFG_ENTRYMODE_HIDE) &&
          (EntryMode(pFieldSplitFileUnit  ) == CFG_ENTRYMODE_HIDE))
         pSeparator->hide();
      Row = 0;
      Col = 1;
      CHK_EXIT (AddField (pDevice, pFieldSplitFileSwitch, pLayoutSplit, &Row, &Col, ForceLastEnteredValues))
      CHK_EXIT (AddField (pDevice, pFieldSplitFileSize  , pLayoutSplit, &Row, &Col, ForceLastEnteredValues))
      CHK_EXIT (AddField (pDevice, pFieldSplitFileUnit  , pLayoutSplit, &Row, &Col, ForceLastEnteredValues))
      pOwn->OldSplitFileSwitch = pFieldSplitFileSwitch->pCheckBox->isChecked();

      if (Duplicate)
      {
         pFieldSplitFileSwitch->pCheckBox->setEnabled(false);
         pFieldSplitFileSize  ->pLineEdit->setEnabled(false);
         pFieldSplitFileUnit  ->pComboBox->setEnabled(false);
      }
      Row = Col = 0;
      for (i=0; i<pDlgAcquireFields->count(); i++)
      {
         pDlgAcquireField = pDlgAcquireFields->at(i);
         if (pDlgAcquireField->EwfField)
            CHK_EXIT (AddField (pDevice, pDlgAcquireField, pLayoutEwf, &Row, &Col, ForceLastEnteredValues))
      }
      pLayoutEwf->setColumnMinimumWidth (0, 20);
   }

   // Other Fields (Userfield)
   // ------------------------
   QGridLayout *pLayoutOther = new QGridLayout ();
   pLayout->addLayout (pLayoutOther);
   Row = Col = 0;
   CHK_EXIT (AddField (pDevice, DlgAcquireGetField(CFG_DLGACQUIRE_USERFIELD), pLayoutOther, &Row, &Col, ForceLastEnteredValues))

   // Destination box
   // ---------------
   QGroupBox   *pGroupBoxDest = new QGroupBox(tr("Destination"), this);
   QGridLayout *pLayoutDest   = new QGridLayout ();
   pGroupBoxDest->setLayout (pLayoutDest);
   pLayout->addWidget (pGroupBoxDest);
   Row = Col = 0;

   if (Clone)
   {
      CHK_EXIT (CreateDeviceTable (pDeviceList, pDevice, &pOwn->pDeviceTable))
      pLayoutDest->addWidget (pOwn->pDeviceTable, Row++, 0, 1, 3);
   }

   if (CONFIG(WriteToDevNull))
   {
      pLabel = new QLabel(QString("<font color='red'>") + tr("Configuration flag WriteToDevNull is set!") + "</font>", this);
      pLayoutDest->addWidget (pLabel, Row++, 0, 1, 3);
   }
   //lint -restore

   for (i=0; i<pDlgAcquireFields->count(); i++)
   {
      pDlgAcquireField = pDlgAcquireFields->at(i);
      if (pDlgAcquireField->DstField)
         CHK_EXIT (AddField (pDevice, pDlgAcquireField, pLayoutDest, &Row, &Col, ForceLastEnteredValues))
   }

   // Hash box
   // --------
   Row = Col = 0;
   QGroupBox   *pGroupBoxHash = new QGroupBox(tr("Hash calculation / verification"), this);
   QGridLayout *pLayoutHash   = new QGridLayout ();
   pGroupBoxHash->setLayout (pLayoutHash);
   pLayout->addWidget (pGroupBoxHash);

   for (i=0; i<pDlgAcquireFields->count(); i++)
   {
      pDlgAcquireField = pDlgAcquireFields->at(i);
      if (pDlgAcquireField->HashField)
      {
         CHK_EXIT (AddField (pDevice, pDlgAcquireField, pLayoutHash, &Row, &Col, ForceLastEnteredValues))
         if (pDlgAcquireField->FieldName.startsWith (CFG_DLGACQUIRE_HASHCALC_FIELDID))
            CHK_QT_EXIT (connect (pDlgAcquireField->pCheckBox, SIGNAL (stateChanged(int)), this, SLOT(UpdateHashState(int))))
         if (Duplicate && (pDlgAcquireField->FieldName.startsWith(CFG_DLGACQUIRE_HASHCALC_FIELDID)))
            pDlgAcquireField->pCheckBox->setEnabled(false);  // Thas hash calculation fields may not be changed in the duplicate dialog
      }
   }

   // Dialog buttons
   // --------------
   QHBoxLayout *pLayoutButtons = new QHBoxLayout (); // The new layout would normally use the margin of its parent layout
   pLayoutButtons->setMargin  (pLayout->margin ());  // (TopLayout), but those have been set to set zero. Instead, we tell
   pLayoutButtons->setSpacing (pLayout->spacing());  // the button layout top use the same margins as the main layout in
   pTopLayout->addLayout (pLayoutButtons);           // order to have everything appear nicely and the same way on the screen).

#ifdef OLD_BUTTON_POSITIONING
   #define ADD_BUTTON(pButton, Text)            \
   {                                            \
      pButton = new QPushButton (Text, this);   \
      pLayoutButtons->addWidget (pButton);      \
   }

   ADD_BUTTON (pOwn->pButtonOk, QObject::tr("Ok"))
   pOwn->pButtonDuplicate = nullptr;
   pOwn->pButtonBack      = nullptr;
   if (Duplicate)
      ADD_BUTTON (pOwn->pButtonBack, tr("Back..."))
   else if (CONFIG(DuplicateImage))
      ADD_BUTTON (pOwn->pButtonDuplicate , tr("Duplicate image..."))
   ADD_BUTTON (pOwn->pButtonCancel, QObject::tr("Cancel"))

   #undef ADD_BUTTON
#endif

   #define ADD_BUTTON(pButton, Text)            \
   {                                            \
      pButton = new QPushButton (Text, this);   \
      pLayoutButtons->addWidget (pButton, 2);   \
   }

   pOwn->pButtonDuplicate = nullptr;
   pOwn->pButtonBack      = nullptr;
   ADD_BUTTON (pOwn->pButtonCancel, QObject::tr("Cancel"))
   pLayoutButtons->addStretch(1);
   if (CONFIG(DuplicateImage))
   {
      if (Duplicate)
           ADD_BUTTON (pOwn->pButtonBack      , tr("Back..."))
      else ADD_BUTTON (pOwn->pButtonDuplicate , tr("Duplicate image..."))
   }
   ADD_BUTTON (pOwn->pButtonOk, QObject::tr("Start"))
   pOwn->pButtonOk->setDefault (true);

   // Set other defaults
   // ------------------
   if (!Clone)
   {
      if (DlgAcquireLastUsedFormat == t_File::NotSet)
         DlgAcquireLastUsedFormat = (t_File::Format) CONFIG (DefaultFormat);

      switch (DlgAcquireLastUsedFormat)
      {
         case t_File::DD  : pOwn->pRadioButtonFormatDD ->setChecked (true); break;
         case t_File::EWF : pOwn->pRadioButtonFormatEWF->setChecked (true); break;
         case t_File::AAFF: pOwn->pRadioButtonFormatAFF->setChecked (true); break;
         default          : CHK_EXIT (ERROR_DLGACQUIRE_INVALID_FORMAT)
      }
   }

   UpdateHashState  ();
   UpdateDialogState();

   // Adjust size
   // -----------
   // Qt limits dialogs to 2/3 of the screen height and width. That's why we adjust it with
   // the following code if the dialog is smaller then its size hint.
   adjustSize();  // Let Qt calculate the size (limited to 2/3 of the screen)
   QRect DialogRect = geometry ();

   if (DialogRect.height() < sizeHint().height())
   {
      QDesktopWidget *pDesktop;

      pDesktop = QApplication::desktop();
      QRect ScreenRect = pDesktop->screenGeometry(pDesktop->screenNumber(this));
      int   NewHeight  = GETMIN (ScreenRect.height()*0.9, sizeHint().height());

      DialogRect.setX      ((ScreenRect.width () - DialogRect.width())/2);
      DialogRect.setY      ((ScreenRect.height() - NewHeight         )/2);
      DialogRect.setHeight (NewHeight);  // Must be done after setY!
      setGeometry(DialogRect);
   }

   // Connections
   // -----------
   if (!Clone)
   {
      CHK_QT_EXIT (connect (pOwn->pRadioButtonFormatDD , SIGNAL (released()), this, SLOT(UpdateFieldState())))
      CHK_QT_EXIT (connect (pOwn->pRadioButtonFormatEWF, SIGNAL (released()), this, SLOT(UpdateFieldState())))
      CHK_QT_EXIT (connect (pOwn->pRadioButtonFormatAFF, SIGNAL (released()), this, SLOT(UpdateFieldState())))
      CHK_QT_EXIT (connect (DlgAcquireGetField ("SplitFileSwitch")->pCheckBox, SIGNAL (released()), this, SLOT(UpdateFileSplitState())))
      UpdateFieldState();
   }
   CHK_QT_EXIT (connect (pOwn->pButtonOk    , SIGNAL (released()), this, SLOT(SlotOk    ())))
   CHK_QT_EXIT (connect (pOwn->pButtonCancel, SIGNAL (released()), this, SLOT(SlotCancel())))
   if (pOwn->pButtonDuplicate)   CHK_QT_EXIT (connect (pOwn->pButtonDuplicate, SIGNAL (released()), this, SLOT(SlotDuplicate())))
   if (pOwn->pButtonBack)        CHK_QT_EXIT (connect (pOwn->pButtonBack     , SIGNAL (released()), this, SLOT(SlotBack     ())))
}

void t_DlgAcquire::UpdateHashState (int /*State*/)
{
   t_pCfgDlgAcquireFields pDlgAcquireFields;
   t_pCfgDlgAcquireField  pDlgAcquireField;
   bool                    Hash = false;
   int                     i;

   CHK_EXIT (CfgGetDlgAcquireFields (&pDlgAcquireFields))
   for (i=0; i<pDlgAcquireFields->count(); i++)
   {
      pDlgAcquireField = pDlgAcquireFields->at(i);
      if (pDlgAcquireField->FieldName.startsWith (CFG_DLGACQUIRE_HASHCALC_FIELDID))
         Hash = Hash || pDlgAcquireField->pCheckBox->isChecked();
   }

   for (i=0; i<pDlgAcquireFields->count(); i++)
   {
      pDlgAcquireField = pDlgAcquireFields->at(i);
      if (pDlgAcquireField->FieldName.startsWith (CFG_DLGACQUIRE_HASHVERIFY_FIELDID))
      {
         if (!Hash)
            pDlgAcquireField->pCheckBox->setChecked (false);
         if (pDlgAcquireField->FieldName == CFG_DLGACQUIRE_HASH_VERIFY_DST)  // The VerifyDest flag may be changed by the user in the duplicate
              pDlgAcquireField->pCheckBox->setEnabled (Hash);                // acquisition dialog, the VerifySrc flag not.
         else pDlgAcquireField->pCheckBox->setEnabled (Hash && !pOwn->Duplicate);
      }
   }
}

void t_DlgAcquire::UpdateDialogState (const QString & /*NewText*/)
{
   t_pCfgDlgAcquireFields pFields;
   t_pCfgDlgAcquireField  pField;
   bool                    Enabled=true;
   int                     i;

   CHK_EXIT (CfgGetDlgAcquireFields (&pFields))

   for (i=0; i<pFields->count() && Enabled; i++)
   {
      pField = pFields->at(i);
      if (pField->DstField)
         Enabled = Enabled && !pField->pLineEdit->text().isEmpty();
   }

   pOwn->pButtonOk->setEnabled (Enabled);
   if (pOwn->pButtonDuplicate)
      pOwn->pButtonDuplicate->setEnabled (Enabled);
}

void t_DlgAcquire::SlotDeviceTableSelectionChanged (void)
{
   t_pDlgAcquireLineEdit        pLineEditDirectory;
   t_pDlgAcquireLineEdit        pLineEditFilename;
   t_DlgAcquireTableWidgetItem *pItem;
   QList<QTableWidgetItem*>      SelectedItemsList = pOwn->pDeviceTable->selectedItems();
   QFileInfo                     FileInfo;
   int                           Row;
   int                           Selected;
   QString                       Directory;

   pLineEditDirectory = DlgAcquireGetField (CFG_DLGACQUIRE_DEST_IMAGEDIRECTORY)->pLineEdit;
   pLineEditFilename  = DlgAcquireGetField (CFG_DLGACQUIRE_DEST_IMAGEFILENAME )->pLineEdit;

   Selected = SelectedItemsList.count();
   if (Selected == 0)
   {
      pLineEditDirectory->setText (QString());
      pLineEditFilename ->setText (QString());
   }
   else
   {
      if (Selected != pOwn->pDeviceTable->columnCount())  // every single item is selected and thus, selected must correspond to the number of columns
      {
         LOG_ERROR ("Clone dialog selection: %d / %d", Selected, pOwn->pDeviceTable->columnCount())
         CHK_EXIT (ERROR_DLGACQUIRE_INVALID_SELECTION)
      }
      Row   = SelectedItemsList[0]->row();
      pItem = dynamic_cast<t_DlgAcquireTableWidgetItem *>(pOwn->pDeviceTable->item (Row, 0)); // Any field contains a pointer to the Device, so simply take the 1st one
      FileInfo.setFile (pItem->GetpDevice()->LinuxDevice);
      Directory = FileInfo.dir().absolutePath();
      if (!Directory.endsWith ("/"))
         Directory += "/";

      pLineEditDirectory->setText (Directory);
      pLineEditFilename ->setText (FileInfo.fileName());
   }

   // No need to call UpdateDialogState, as the LineEdit widget will emit a textChanged signal which is connected to UpdateDialogState.
}

void t_DlgAcquire::UpdateFieldState (void)
{
   QWidget *pWidget;
   bool      Enabled;
   int       i;

   // EWF fields
   // ----------
   Enabled =  pOwn->pRadioButtonFormatEWF->isChecked() ||
              pOwn->pRadioButtonFormatAFF->isChecked() ||
             (pOwn->pRadioButtonFormatDD ->isChecked() && CONFIG (InfoFieldsForDd));
   for (i = 0;
        i < pOwn->EwfWidgetsList.count();
        i++)
   {
      pWidget = pOwn->EwfWidgetsList.at(i);
      pWidget->setEnabled (Enabled);
   }

   // Split file fields
   // -----------------
   t_pCfgDlgAcquireField pFieldSplitFileSwitch = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILESWITCH);
   t_pCfgDlgAcquireField pFieldSplitFileSize   = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILESIZE  );
   t_pCfgDlgAcquireField pFieldSplitFileUnit   = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILEUNIT  );

   if (pOwn->pRadioButtonFormatDD->isChecked())
   {
      pFieldSplitFileSwitch->pCheckBox->setChecked (pOwn->OldSplitFileSwitch);
      pFieldSplitFileSwitch->pCheckBox->setEnabled (!pOwn->Duplicate);
   }
   else if (pOwn->pRadioButtonFormatEWF->isChecked())
   {
      if (pFieldSplitFileSwitch->pCheckBox->isEnabled())
         pOwn->OldSplitFileSwitch = pFieldSplitFileSwitch->pCheckBox->isChecked();
      pFieldSplitFileSwitch->pCheckBox->setChecked (true);
      pFieldSplitFileSwitch->pCheckBox->setEnabled (false);
   }
   else if (pOwn->pRadioButtonFormatAFF->isChecked())
   {
      if (pFieldSplitFileSwitch->pCheckBox->isEnabled())
         pOwn->OldSplitFileSwitch = pFieldSplitFileSwitch->pCheckBox->isChecked();
      pFieldSplitFileSwitch->pCheckBox->setChecked (false);
      pFieldSplitFileSwitch->pCheckBox->setEnabled (false);
   }
   Enabled = pFieldSplitFileSwitch->pCheckBox->isChecked() && !pOwn->Duplicate;
   pFieldSplitFileSize->pLineEdit->setEnabled (Enabled);
   pFieldSplitFileSize->pLabel   ->setEnabled (Enabled);
   pFieldSplitFileUnit->pComboBox->setEnabled (Enabled);
}

void t_DlgAcquire::UpdateFileSplitState (void)
{
   t_pCfgDlgAcquireField pFieldSplitFileSwitch = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILESWITCH);
   t_pCfgDlgAcquireField pFieldSplitFileSize   = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILESIZE  );
   t_pCfgDlgAcquireField pFieldSplitFileUnit   = DlgAcquireGetField (CFG_DLGACQUIRE_SPLITFILEUNIT  );

   bool Enabled = pFieldSplitFileSwitch->pCheckBox->isChecked() && !pOwn->Duplicate;
   pFieldSplitFileSize->pLineEdit->setEnabled (Enabled);
   pFieldSplitFileSize->pLabel   ->setEnabled (Enabled);
   pFieldSplitFileUnit->pComboBox->setEnabled (Enabled);
}

void t_DlgAcquire::SlotBrowse (void)
{
   t_pDlgAcquireLineEdit pLineEdit;
   QString                Path;
   bool                   OkPressed;

   pLineEdit = (t_pDlgAcquireLineEdit) sender()->property (DLGACQUIRE_PROPERTY_SENDER_LINEEDIT).value<void *>();

   if (CONFIG(UseFileDialogFromQt))
   {
      Path = QFileDialog::getExistingDirectory(this, tr("Select destination directory", "Dialog title"), pLineEdit->text(), QFileDialog::ShowDirsOnly);
      OkPressed = !Path.isNull();
   }
   else
   {
      Path = pLineEdit->text();
      t_DlgDirSel Dlg (&Path, this);

      switch (CONFIG(FileDialogSize))
      {
         case CFG_STARTUPSIZE_STANDARD  :                       break;
         case CFG_STARTUPSIZE_FULLSCREEN: Dlg.showFullScreen(); break;
         case CFG_STARTUPSIZE_MAXIMIZED : Dlg.showMaximized (); break;
         case CFG_STARTUPSIZE_MANUAL    : CHK_EXIT (QtUtilSetGeometryCentered (&Dlg, CONFIG(FileDialogSizeManualDx), CONFIG(FileDialogSizeManualDy))); break;
         default                        : CHK_EXIT (ERROR_DLGACQUIRE_UNKNOWN_FILEDIALOG_SIZE)
      }
      OkPressed = (Dlg.exec() == QDialog::Accepted);
   }

   if (OkPressed)
   {
      if (!Path.endsWith ("/"))
         Path += "/";
      pLineEdit->setText (Path); // setText doesn't emit a textEdited signal (that's completely right  in order to prevent endless
      pLineEdit->TextUpdated (); // update signals) and so we have the LineEdit emit a SignalTextEdit, thus triggering field updates.
   }
}

void t_DlgAcquire::SlotTextEdited (t_pDlgAcquireLineEdit pLineEdit, const QString &/*NewVal*/)
{
   t_pCfgDlgAcquireRules pDlgAcquireRules;
   t_pCfgDlgAcquireRule  pDlgAcquireRule;
   t_pDlgAcquireLineEdit pLineEditDest;
   int                    i;
   QString                Set;

//   printf ("\nNewVal in %s: %s", QSTR_TO_PSZ(pLineEdit->Name), QSTR_TO_PSZ(NewVal));

   CHK_EXIT (CfgGetDlgAcquireRules (&pDlgAcquireRules))

   for (i=0; i<pDlgAcquireRules->count(); i++)
   {
      pDlgAcquireRule = pDlgAcquireRules->at(i);
      if (pDlgAcquireRule->TriggerFieldName.compare (pLineEdit->Name, Qt::CaseInsensitive) == 0)
      {
         pLineEditDest = DlgAcquireGetLineEdit (pDlgAcquireRule->DestFieldName);

         CHK_EXIT (DlgAcquireResolveSpecialSequences (pOwn->pDevice, true, pDlgAcquireRule->Value, Set))
         pLineEditDest->setText(Set);
      }
   }
}

static APIRET DlgAcquireCheckFilename (const QString &In, bool &Clean, QString &Out)
{
   unsigned char Ch;
   int           i;
   bool          Ok;
   QString       SpecialChars = CONFIG (SpecialFilenameChars);

   Clean = true;
   Out   = "";
   for (i=0; i<In.length(); i++)
   {
      Ch = In[i].toLatin1();
      Ok = ((Ch >= '0') && (Ch <= '9'))                 ||
           ((Ch >= 'a') && (Ch <= 'z'))                 ||
           ((Ch >= 'A') && (Ch <= 'Z'))                 ||
            (Ch == '_')                                 ||
           ((Ch == '/') && CONFIG(AllowPathInFilename)) ||
           SpecialChars.contains(Ch);
      if (Ok)
           Out += Ch;
      else Clean = false;
   }
   if (Out.isEmpty())
      Out = DLGACQUIRE_DEFAULT_EMPTY_FILENAME;

   return NO_ERROR;
}

// AdjustPathAndFilename analyses Path and Filename and separates/reassembles them correctly. This
// function became neccessary with the introduction of relative paths in the filename field (configuration
// parameter AllowPathInFilename)
// Example:   input - Path     = /home             |     output - Path     = /home/vogu/aa/
//                    Filename = vogu/aa/test      |              Filename = test
//

APIRET t_DlgAcquire::AdjustPathAndFilename (QString &Path, QString &Filename)
{
   int LastSlash;

   if (!Path.endsWith ('/'))
      Path += '/';

   if (Filename.startsWith ('/'))
      Filename.remove (0,1);

   LastSlash = Filename.lastIndexOf ('/');
   if (LastSlash >= 0)
   {
       Path += Filename.left (LastSlash+1);
       Filename.remove (0, LastSlash+1);
   }
   return NO_ERROR;
}

APIRET t_DlgAcquire::CheckWriteAccess (t_pcDevice pDevice, const QString &Path, const QString &Filename, bool &Ok)
{
   FILE       *pFile;
   const char *pStr = "Test file created by Guymager for checking write access.\r\nYou may delete this file";
   const int    StrLen = strlen (pStr);
   QString      TestFileName;
   int          wr;
   bool         DirOk;
   QMessageBox::StandardButton  Button;

   TestFileName = Path + Filename + ".test";

   // Create directory if it doesn't exist
   // ------------------------------------
   QDir Dir(Path);

   if (Dir.exists())
   {
      LOG_INFO ("[%s] Directory %s exists", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(Path))
   }
   else
   {
      if (CONFIG(ConfirmDirectoryCreation))
      {
         LOG_INFO ("[%s] Directory %s doesn't exist. Asking user if it should be created.", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(Path))
         Button = t_MessageBox::question (this, tr ("Path not found", "Dialog title"),
                                                tr ("The destination directory"
                                                    "\n   %1"
                                                    "\ndoesn't exist. Do you want to create it?") .arg(Path), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
         Ok = Button == QMessageBox::Yes;
         if (!Ok)
         {
            LOG_INFO ("[%s] User cancels directory creation", QSTR_TO_PSZ(pDevice->LinuxDevice))
            return NO_ERROR;
         }
         LOG_INFO ("[%s] User agrees to create directory", QSTR_TO_PSZ(pDevice->LinuxDevice))
      }
      LOG_INFO ("[%s] Creating directory %s.", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(Path))
      DirOk = Dir.mkpath(Path); // Corresponds to mkdir -p
      if (DirOk)
         DirOk = Dir.exists();

      if (DirOk)
           LOG_INFO ("[%s] Directory %s created successfully", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(Path))
      else LOG_INFO ("[%s] Error while creating directory %s", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(Path))
   }

   LOG_INFO ("[%s] Trying write access to test file %s", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(TestFileName))
   pFile = fopen (QSTR_TO_PSZ(TestFileName), "w");
   Ok = (pFile != nullptr);
   if (!Ok)
   {
      LOG_INFO ("[%s] Opening test file %s failed", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(TestFileName))
   }
   else
   {
      if ((wr = fprintf (pFile, "%s", pStr)) != StrLen)
      {
         Ok = false;
         LOG_INFO ("[%s] Writing to test file %s failed, %d of %d bytes written", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(TestFileName), wr, StrLen)
      }

      if (fclose (pFile) != 0)
      {
         Ok = false;
         LOG_INFO ("[%s] Closing test file %s failed", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(TestFileName))
      }

      QDir          Dir(Path);                                                                                           // Check if the test file really exists with that
      QFileInfoList FileInfoList = Dir.entryInfoList (QStringList(Filename + ".test"), QDir::Files | QDir::NoSymLinks);  // name (Otherwise, strange things may happen with
      if (FileInfoList.isEmpty())                                                                                        // the : character, as it used for alternate data
      {                                                                                                                  // streams in some DOS-based OSs.
         Ok = false;
         LOG_INFO ("[%s] The test file %s doesn't exist with the correct name", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(TestFileName))
      }

      if (!QFile::remove (TestFileName))
      {
         Ok = false;
         LOG_INFO ("[%s] Removing the test file %s failed", QSTR_TO_PSZ(pDevice->LinuxDevice), QSTR_TO_PSZ(TestFileName))
      }
   }

   if (Ok)
        LOG_INFO ("[%s] Write access test succeeded", QSTR_TO_PSZ(pDevice->LinuxDevice))
   else t_MessageBox::information (this, tr ("Access denied", "Dialog title"),
                                         tr ("Guymager cannot write to the directory"
                                            "\n\t%1"
                                            "\nThis may be due to insufficient access rights or unsupported filename characters. Please choose another directory.")
                                            .arg(Path), QMessageBox::Ok);
   return NO_ERROR;
}

static QString DlgAcquireCanonicalPath (const QString &Path, const QString &Filename)
{                                                        // Do not use QFileInfo::canonicalFilePath, as it returns an empty string for non-existent files. As well,
   return QDir(Path).canonicalPath() + "/" + Filename;   // QDir::canonicalPath returns an empty string for non-existent dirs if doesn't exist, see remarks below where
}                                                        // this fn is used.

void t_DlgAcquire::SlotOk        (void) { Accept (false); }
void t_DlgAcquire::SlotDuplicate (void) { Accept (true ); }
void t_DlgAcquire::SlotBack      (void) { done (t_DlgAcquire::ButtonBack  ); }
void t_DlgAcquire::SlotCancel    (void) { done (t_DlgAcquire::ButtonCancel); }

void t_DlgAcquire::Accept (bool DuplicatePressed)
{
   t_Acquisition                Acquisition (nullptr);
   t_pDevice                   pDevice;
   t_pDlgAcquireLineEdit       pLineEditImageDirectory         , pLineEditImageFilename         , pLineEditInfoDirectory         , pLineEditInfoFilename;
   QString                              ImageDirectoryCorrected,          ImageFilenameCorrected,          InfoDirectoryCorrected,          InfoFilenameCorrected;
   bool                                 ImageDirectoryClean    ,          ImageFilenameClean    ,          InfoDirectoryClean    ,          InfoFilenameClean;
   QString                      Message;
   QMessageBox::StandardButton  Button;
   unsigned long long           SplitSize = 0;

   LOG_INFO ("[%s] Acquisition dialog accept - %s pressed", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice), DuplicatePressed ? "Duplicate" : "Ok")

   // Check split file size
   // ---------------------
   if (!pOwn->Clone)
   {
      if (DlgAcquireGetField(CFG_DLGACQUIRE_SPLITFILESWITCH)->pCheckBox->isChecked())
      {
         QString StrValue;
         double  NumValue;
         bool    Ok;

         StrValue = DlgAcquireGetField(CFG_DLGACQUIRE_SPLITFILESIZE)->pLineEdit->text();
         NumValue = StrValue.toDouble(&Ok);
         if (!Ok)
         {
            t_MessageBox::information (this, tr ("Incorrect value", "Dialog title"),
                                             tr ("The split file size \"%1\" is not valid. Only positive numbers can be entered.").arg(StrValue), QMessageBox::Ok);
            return;
         }
         SplitSize = NumValue * UnitIndexToMultiplier (DlgAcquireGetField(CFG_DLGACQUIRE_SPLITFILEUNIT)->pComboBox->currentIndex());
         if (pOwn->pRadioButtonFormatEWF->isChecked())
         {
            #if (ENABLE_LIBEWF)
               if ((CONFIG(EwfFormat) == LIBEWF_FORMAT_ENCASE6) ||
                   (CONFIG(EwfFormat) == t_File::AEWF))
            #else
               if (CONFIG(EwfFormat) == t_File::AEWF)
            #endif
            {
               if ((SplitSize > EWF_MAX_SEGMENT_SIZE_EXT) || (SplitSize < EWF_MIN_SEGMENT_SIZE))
               {
                  t_MessageBox::information (this, tr ("Incorrect value", "Dialog title"),
                                                   tr ("The split size for the selected image format must be in the range %1MiB - %2EiB.")
                                                   .arg(EWF_MIN_SEGMENT_SIZE    / (1024*1024))
                                                   .arg(EWF_MAX_SEGMENT_SIZE_EXT/ (double)BYTES_PER_EiB, 0, 'f', 2), QMessageBox::Ok);
                  return;
               }
            }
            else
            {
               if ((SplitSize > EWF_MAX_SEGMENT_SIZE) || (SplitSize < EWF_MIN_SEGMENT_SIZE))
               {
                  // function libewf_segment_file_write_chunks_section_correction() only uses INT64_MAX chunk size for Encase6 not Encase7
                  t_MessageBox::information (this, tr ("Incorrect value", "Dialog title"),
                                                   tr ("The split size for the configured EWF format must be in the range %1 - %2 MiB. "
                                                       "For bigger files, switch to ""Encase6"" format or ""Guymager"" (see Guymager configuration file, parameter EwfFormat).")
                                                   .arg(EWF_MIN_SEGMENT_SIZE/(1024*1024))
                                                   .arg(EWF_MAX_SEGMENT_SIZE/(1024*1024)), QMessageBox::Ok);
                  return;
               }
            }
         }
      }
   }

   // Check for strange characters in directories and filenames
   // ---------------------------------------------------------
   pLineEditImageDirectory = DlgAcquireGetField (CFG_DLGACQUIRE_DEST_IMAGEDIRECTORY)->pLineEdit;
   pLineEditImageFilename  = DlgAcquireGetField (CFG_DLGACQUIRE_DEST_IMAGEFILENAME )->pLineEdit;
   pLineEditInfoDirectory  = DlgAcquireGetField (CFG_DLGACQUIRE_DEST_INFODIRECTORY )->pLineEdit;
   pLineEditInfoFilename   = DlgAcquireGetField (CFG_DLGACQUIRE_DEST_INFOFILENAME  )->pLineEdit;

   CHK_EXIT (DlgAcquireCheckFilename (pLineEditImageDirectory->text(), ImageDirectoryClean, ImageDirectoryCorrected))  // Check all image/info directories and
   CHK_EXIT (DlgAcquireCheckFilename (pLineEditImageFilename ->text(), ImageFilenameClean , ImageFilenameCorrected ))  // filenames and then override those that
   CHK_EXIT (DlgAcquireCheckFilename (pLineEditInfoDirectory ->text(), InfoDirectoryClean , InfoDirectoryCorrected ))  // should not be checked (it's easier to do
   CHK_EXIT (DlgAcquireCheckFilename (pLineEditInfoFilename  ->text(), InfoFilenameClean  , InfoFilenameCorrected  ))  // it that way then with a lot of if/else)

   if (pOwn->Clone)
   {
      ImageDirectoryClean = true;
      ImageFilenameClean  = true;
   }
   if (!CONFIG (DirectoryFieldEditing))  // Directories selected via file dialog are always good and must
   {                                     // not be checked/replaced (check would like to replace slashes)
      ImageDirectoryClean = true;
      InfoDirectoryClean  = true;
   }

   if (!ImageDirectoryClean || !ImageFilenameClean || !InfoDirectoryClean || !InfoFilenameClean)
   {
      LOG_INFO ("[%s] Unallowed characters in filenames or directories found", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
      Message = tr ("The directory or file names contain special characters which are not allowed. Guymager suggests the following changes:");
      if (!ImageDirectoryClean)                                                                       Message += tr ("\n   Image directory:\t%1") .arg (ImageDirectoryCorrected);
      if (!ImageFilenameClean )                                                                       Message += tr ("\n   Image filename:\t%1" ) .arg (ImageFilenameCorrected );
      if (!InfoDirectoryClean && (pLineEditInfoDirectory->text() != pLineEditImageDirectory->text())) Message += tr ("\n   Info directory:\t%1" ) .arg (InfoDirectoryCorrected );
      if (!InfoFilenameClean  && (pLineEditInfoFilename ->text() != pLineEditImageFilename ->text())) Message += tr ("\n   Info filename:\t%1"  ) .arg (InfoFilenameCorrected  );

      Message += tr ("\nDo you accept these changes?");
      Button = t_MessageBox::question (this, tr ("Special characters", "Dialog title"), Message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
      if (Button == QMessageBox::Yes)
      {
         LOG_INFO ("[%s] User accepts filename changes", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
         if (!ImageDirectoryClean) pLineEditImageDirectory->setText (ImageDirectoryCorrected);
         if (!ImageFilenameClean ) pLineEditImageFilename ->setText (ImageFilenameCorrected );
         if (!InfoDirectoryClean ) pLineEditInfoDirectory ->setText (InfoDirectoryCorrected );
         if (!InfoFilenameClean  ) pLineEditInfoFilename  ->setText (InfoFilenameCorrected  );
      }
      else
      {
         LOG_INFO ("[%s] User rejects filename changes", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
         return;
      }
   }

   // Check if duplicate image or info filename is identical to image name of first acquisition
   // ------------------------------------------------------------------------------------------
   CHK_EXIT (GetParameters (Acquisition, false))

   if (pOwn->Duplicate && pOwn->pAcquisition1) // Both conditions normally are always both true or both false, but let's check both here just to be sure
   {
      QString ImageFilename1 = DlgAcquireCanonicalPath (pOwn->pAcquisition1->ImagePath, pOwn->pAcquisition1->ImageFilename);
      QString InfoFilename1  = DlgAcquireCanonicalPath (pOwn->pAcquisition1->ImagePath, pOwn->pAcquisition1->InfoFilename );
      QString ImageFilename2 = DlgAcquireCanonicalPath (Acquisition.ImagePath, Acquisition.ImageFilename);
      QString InfoFilename2  = DlgAcquireCanonicalPath (Acquisition.ImagePath, Acquisition.InfoFilename );
      // DlgAcquireCanonicalPath only returns the filename if the path doesn't exist (see above). As the path of Acquisition1 surely exists
      // (created by t_DlgAcquire::CheckWriteAccess during the 1st acquisition dialog if it wasn't there yet), the string comparison following
      // below returns a correct result even if the user chosed a non-existent path for the duplicate acquisition.

      if (ImageFilename1.toUpper() == ImageFilename2.toUpper())
      {
         t_MessageBox::information (this, tr ("Destination in use", "Dialog title"),
                                          tr ("The destination specified already is used for the first image."),
                                          QMessageBox::Ok);
         return;
      }
      if (InfoFilename1.toUpper() == InfoFilename2.toUpper())
      {
         t_MessageBox::information (this, tr ("Info file in use", "Dialog title"),
                                          tr ("The info file specified already is used for the first image."),
                                          QMessageBox::Ok);
         return;
      }
   }

   // Check if file can be created on destination path
   // ------------------------------------------------
   bool Ok;

   if (!pOwn->Clone)
   {
      CHK_EXIT (t_DlgAcquire::CheckWriteAccess (pOwn->pDevice, Acquisition.InfoPath, Acquisition.InfoFilename, Ok))
      if (Ok && ((Acquisition.InfoPath     != Acquisition.ImagePath    ) ||
                 (Acquisition.InfoFilename != Acquisition.ImageFilename)))
         CHK_EXIT (t_DlgAcquire::CheckWriteAccess (pOwn->pDevice, Acquisition.ImagePath, Acquisition.ImageFilename, Ok))
      if (!Ok)
         return;
   }

   // Check if another acquisition uses the same image or info files
   // --------------------------------------------------------------
   bool Used=false;
   #define CHK_PATH(Path1,File1,Path2,File2)           \
   {                                                   \
      Used = DlgAcquireCanonicalPath (Path1, File1) == \
             DlgAcquireCanonicalPath (Path2, File2);   \
      if (Used)                                        \
         break;                                        \
   }

   for (int i=0; i<pOwn->pDeviceList->count(); i++)
   {
      pDevice = pOwn->pDeviceList->at(i);

      if (pDevice == pOwn->pDevice)                      // Do not check the device that user wants to acquire
         continue;
      if ((pDevice->GetState() == t_Device::Idle   ) ||
          (pDevice->GetState() == t_Device::Aborted))    // Files from an aborted acquisition can be overwritten
         continue;
      CHK_PATH (pDevice->Acquisition1.ImagePath, pDevice->Acquisition1.ImageFilename, Acquisition.ImagePath, Acquisition.ImageFilename);
      CHK_PATH (pDevice->Acquisition1.InfoPath , pDevice->Acquisition1.InfoFilename , Acquisition.InfoPath , Acquisition.InfoFilename );
      if (pDevice->Duplicate)
      {
         CHK_PATH (pDevice->Acquisition2.ImagePath, pDevice->Acquisition2.ImageFilename, Acquisition.ImagePath, Acquisition.ImageFilename);
         CHK_PATH (pDevice->Acquisition2.InfoPath , pDevice->Acquisition2.InfoFilename , Acquisition.InfoPath , Acquisition.InfoFilename );
      }
   }
   if (Used)
   {
      LOG_INFO ("[%s] Another acquisition (%s) is using the same file names", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice),
                                                                              QSTR_TO_PSZ(pDevice->LinuxDevice))
      Button = t_MessageBox::warning (this, tr ("Files already used", "Dialog title"),
                                                tr ("The image or info files already are used in the acquisition of %1. Please chose different ones.") .arg(pDevice->LinuxDevice),
                                                QMessageBox::Ok);
      LOG_INFO ("[%s] User closes info dialog", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
      return;
   }
   #undef CHK_PATH

   // Check if image file already exists
   // ----------------------------------

   QDir    Dir (Acquisition.ImagePath);
   QString ExtensionImage;
   bool    Empty;
   QString NameFilter;
   int     DdSplitDecimals=0;

   if (pOwn->Clone)
   {
      Empty = true;  // No image file when cloning drives, so no check
   }
   else
   {                 // Check if image file with name/extension already exists
      DdSplitDecimals = t_File::GetDdSplitNrDecimals (pOwn->pDevice->Size, Acquisition.SplitFileSize);
      CHK_EXIT (t_File::GetFormatExtension (Acquisition.Format, Acquisition.Clone, DdSplitDecimals, &ExtensionImage))
      NameFilter = Acquisition.ImageFilename + ExtensionImage;
      Empty = Dir.entryInfoList (QStringList(NameFilter), QDir::Files, QDir::Name).isEmpty();
   }
   if (Empty)
   {                 // Check if info file already exists
      Dir.setPath (Acquisition.InfoPath);
      NameFilter = Acquisition.InfoFilename + t_File::pExtensionInfo;
      Empty = Dir.entryInfoList (QStringList(NameFilter), QDir::Files, QDir::Name).isEmpty();
   }

   if (!Empty)
   {
      LOG_INFO ("[%s] Image file / info files already exist", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
      Button = t_MessageBox::question (this, tr ("Files exist", "Dialog title"),
                                       pOwn->Clone ? tr ("The info file already exists. Do you want to overwrite it?") :
                                                     tr ("The image or info files already exist. Do you want to overwrite them?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (Button == QMessageBox::Yes)
      {
         LOG_INFO ("[%s] User accepts to overwrite existing image/info files", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
      }
      else
      {
         LOG_INFO ("[%s] User rejects to overwrite existing image/info files", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
         return;
      }
   }

   // Check number of EWF segment files
   // ---------------------------------
   if (CONFIG (WarnAboutSegmentFileCount)        &&
       !pOwn->Clone                              &&
        pOwn->pRadioButtonFormatEWF->isChecked() &&
       (DlgAcquireGetField(CFG_DLGACQUIRE_SPLITFILESWITCH)->pCheckBox->isChecked()))
   {
      quint64 Segments = (pOwn->pDevice->Size / SplitSize);

      if (Segments > 14971)  // (E01-E99)->99 + (EAA-EZZ)->26*26 + (FAA-ZZZ) -> 26*26*21 = 14.971
      {
         LOG_INFO ("[%s] This image could include up to %llu segment files (beyond extension ZZZ)", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice), Segments)
         Button = t_MessageBox::question (this, tr ("Large amount of segment files", "Dialog title"),
                                          tr ("With the chosen segment size, up to %1 image segment files could possibly be created. "
                                              "This would result in file extensions beyond .ZZZ"
                                              "\nDo you accept such a large amount of files?") .arg (Segments),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
         if (Button == QMessageBox::Yes)
         {
            LOG_INFO ("[%s] User accepts large amount of segment files", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
         }
         else
         {
            LOG_INFO ("[%s] User refuses large amount of segment files", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
            return;
         }
      }
   }

   // Check free space on destination
   // -------------------------------
   if (!pOwn->Clone &&
        CONFIG (WarnAboutImageSize))
   {
      quint64 FreeBytes;
      APIRET  rc;

      rc = UtilGetFreeSpace (Acquisition.ImagePath, &FreeBytes);
      if (rc == NO_ERROR)
      {
         if (pOwn->pDevice->Size > FreeBytes)
         {
            LOG_INFO ("[%s] Free space on %s is %llu which is less than source device size (%llu)", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice), QSTR_TO_PSZ(Acquisition.ImagePath), FreeBytes, pOwn->pDevice->Size);
            Button = t_MessageBox::question (this, tr ("Free space", "Dialog title"),
                                             tr ("There possibly is not enough space left on the destination path.\n"
                                                 "Source data size: %1\n"
                                                 "Free space on destination: %2\n"
                                                 "\nDo you want to continue anyway?") .arg(UtilSizeToHuman (pOwn->pDevice->Size))
                                                                                      .arg(UtilSizeToHuman (FreeBytes)),
                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (Button == QMessageBox::Yes)
            {
               LOG_INFO ("[%s] User ignores free space problems and continues", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
            }
            else
            {
               LOG_INFO ("[%s] User doesn't start acquisition because of low space in destination path", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice))
               return;
            }

         }
      }
      else
      {
         LOG_INFO ("[%s] The amount of free space on %s cannot be determined", QSTR_TO_PSZ(pOwn->pDevice->LinuxDevice), QSTR_TO_PSZ(Acquisition.ImagePath));
      }
   }

   done(DuplicatePressed ? t_DlgAcquire::ButtonDuplicate : t_DlgAcquire::ButtonOk);
}


APIRET t_DlgAcquire::GetParameters (t_Acquisition &Acquisition, bool RememberLastUsedValues)
{
   t_pCfgDlgAcquireField pDlgAcquireField;
   bool                   SplitFileSwitch;
   QString                SplitFileSize = "0";

   Acquisition.Clone = pOwn->Clone;
   if (pOwn->Clone)
   {
      Acquisition.Format = t_File::DD;
   }
   else
   {
      if      (pOwn->pRadioButtonFormatDD ->isChecked()) Acquisition.Format = t_File::DD;
      else if (pOwn->pRadioButtonFormatEWF->isChecked()) Acquisition.Format = t_File::EWF;
      else                                               Acquisition.Format = t_File::AAFF;
      if (RememberLastUsedValues)
         DlgAcquireLastUsedFormat = Acquisition.Format;
   }

   #define COPY_LINEENTRY(Acq, FieldName, Remember)      \
   {                                                     \
      pDlgAcquireField = DlgAcquireGetField (FieldName); \
      Acq = pDlgAcquireField->pLineEdit->text();         \
      if (Remember)                                      \
         pDlgAcquireField->LastEnteredValue = Acq;       \
   }

   #define COPY_CHECKBOX(Acq, FieldName, Remember)       \
   {                                                     \
      pDlgAcquireField = DlgAcquireGetField (FieldName); \
      Acq = pDlgAcquireField->pCheckBox->isChecked();    \
      if (Remember)                                      \
         pDlgAcquireField->LastEnteredValue = Acq  ? "1" : "0"; \
   }

   #define COPY_COMBOBOX(Acq, FieldName, Remember)       \
   {                                                     \
      pDlgAcquireField = DlgAcquireGetField (FieldName); \
      Acq = pDlgAcquireField->pComboBox->currentIndex(); \
      if (Remember)                                      \
         pDlgAcquireField->LastEnteredValue = QString::number (Acq); \
   }

   if (!pOwn->Clone)
   {
      COPY_LINEENTRY (Acquisition.CaseNumber    , CFG_DLGACQUIRE_EWF_CASENUMBER     , RememberLastUsedValues)
      COPY_LINEENTRY (Acquisition.EvidenceNumber, CFG_DLGACQUIRE_EWF_EVIDENCENUMBER , RememberLastUsedValues)
      COPY_LINEENTRY (Acquisition.Examiner      , CFG_DLGACQUIRE_EWF_EXAMINER       , RememberLastUsedValues)
      COPY_LINEENTRY (Acquisition.Description   , CFG_DLGACQUIRE_EWF_DESCRIPTION    , RememberLastUsedValues)
      COPY_LINEENTRY (Acquisition.Notes         , CFG_DLGACQUIRE_EWF_NOTES          , RememberLastUsedValues)
      COPY_CHECKBOX  (SplitFileSwitch           , CFG_DLGACQUIRE_SPLITFILESWITCH    , RememberLastUsedValues)
      if (SplitFileSwitch)
      {
         int UnitIndex;
         COPY_LINEENTRY (SplitFileSize, CFG_DLGACQUIRE_SPLITFILESIZE, RememberLastUsedValues)
         COPY_COMBOBOX  (UnitIndex    , CFG_DLGACQUIRE_SPLITFILEUNIT, RememberLastUsedValues)
         Acquisition.SplitFileSize = SplitFileSize.toULongLong() * UnitIndexToMultiplier(UnitIndex);
      }
      else
      {
         Acquisition.SplitFileSize = 0;
      }
   }
   COPY_LINEENTRY (Acquisition.UserField     , CFG_DLGACQUIRE_USERFIELD          , RememberLastUsedValues)
   COPY_LINEENTRY (Acquisition.ImagePath     , CFG_DLGACQUIRE_DEST_IMAGEDIRECTORY, pOwn->Clone ? false : RememberLastUsedValues)
   COPY_LINEENTRY (Acquisition.ImageFilename , CFG_DLGACQUIRE_DEST_IMAGEFILENAME , pOwn->Clone ? false : RememberLastUsedValues)
   COPY_LINEENTRY (Acquisition.InfoPath      , CFG_DLGACQUIRE_DEST_INFODIRECTORY , RememberLastUsedValues)
   COPY_LINEENTRY (Acquisition.InfoFilename  , CFG_DLGACQUIRE_DEST_INFOFILENAME  , RememberLastUsedValues)
   CHK (AdjustPathAndFilename (Acquisition.ImagePath, Acquisition.ImageFilename))
   CHK (AdjustPathAndFilename (Acquisition.InfoPath , Acquisition.InfoFilename ))

   COPY_CHECKBOX (Acquisition.CalcMD5   , CFG_DLGACQUIRE_HASH_CALC_MD5   , RememberLastUsedValues)
   COPY_CHECKBOX (Acquisition.CalcSHA1  , CFG_DLGACQUIRE_HASH_CALC_SHA1  , RememberLastUsedValues)
   COPY_CHECKBOX (Acquisition.CalcSHA256, CFG_DLGACQUIRE_HASH_CALC_SHA256, RememberLastUsedValues)
   COPY_CHECKBOX (Acquisition.VerifySrc , CFG_DLGACQUIRE_HASH_VERIFY_SRC , RememberLastUsedValues)
   COPY_CHECKBOX (Acquisition.VerifyDst , CFG_DLGACQUIRE_HASH_VERIFY_DST , RememberLastUsedValues)

   #undef COPY_LINEENTRY
   #undef COPY_CHECKBOX

   return NO_ERROR;
}

t_DlgAcquire::~t_DlgAcquire ()
{
   delete pOwn;
}
