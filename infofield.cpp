// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         The info field area
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

#include "toolconstants.h"

#include "common.h"
#include "main.h"
#include "config.h"
#include "qtutil.h"
#include "infofield.h"

class t_InfoFieldLocal
{
   public:
      t_pDeviceList pDeviceList;
      QLabel       *pLabelParams;
      QLabel       *pLabelValues;
};

t_InfoField::t_InfoField ()
{
   CHK_EXIT (ERROR_INFOFIELD_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 not initialised

t_InfoField::t_InfoField (QWidget *pParent, t_pDeviceList pDeviceList)
   :QFrame (pParent)
{
   CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_INFOFIELD_CONSTRUCTOR_NOT_SUPPORTED))

   setFrameStyle ((int)QFrame::Panel | (int)QFrame::Sunken);
   pOwn = new t_InfoFieldLocal;
   pOwn->pDeviceList = pDeviceList;

   QHBoxLayout *pLayout = new QHBoxLayout (this);
   pOwn->pLabelValues = new QLabel (this);
   pOwn->pLabelParams = new QLabel (this);
   QTUTIL_SET_FONT (pOwn->pLabelValues, FONTOBJECT_INFOFIELD)
   QTUTIL_SET_FONT (pOwn->pLabelParams, FONTOBJECT_INFOFIELD)
   pLayout->addWidget (pOwn->pLabelParams);
   pLayout->addWidget (pOwn->pLabelValues);
   pLayout->addStretch();
}

t_InfoField::~t_InfoField ()
{
   delete pOwn->pLabelParams;
   delete pOwn->pLabelValues;
   delete pOwn;
}

QString t_InfoField::GetHashText (bool MD5, bool SHA1, bool SHA256)
{
   QString Str;

   if      (!MD5 && !SHA1 && !SHA256) Str = t_InfoField::tr("off"          , "Hash calculation is off");
   else if (!MD5 && !SHA1 &&  SHA256) Str = t_InfoField::tr("%1"           , "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("SHA-256");
   else if (!MD5 &&  SHA1 && !SHA256) Str = t_InfoField::tr("%1"           , "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("SHA-1");
   else if (!MD5 &&  SHA1 &&  SHA256) Str = t_InfoField::tr("%1 and %2"    , "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("SHA-1") .arg("SHA-256");
   else if ( MD5 && !SHA1 && !SHA256) Str = t_InfoField::tr("%1"           , "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("MD5");
   else if ( MD5 && !SHA1 &&  SHA256) Str = t_InfoField::tr("%1 and %2"    , "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("MD5") .arg("SHA-256");
   else if ( MD5 &&  SHA1 && !SHA256) Str = t_InfoField::tr("%1 and %2"    , "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("MD5") .arg("SHA-1");
   else                               Str = t_InfoField::tr("%1, %2 and %3", "Show which hash values are in use, % value wll be replaced by MD5, SHA-1, ...") .arg("MD5") .arg("SHA-1") .arg("SHA-256");
   return Str;
}

void t_InfoField::SlotShowInfo (t_pDevice pDevice)
{
   QString StrValue, StrSpeed, Format;
   quint64 Size;
   QString LabelParams;
   bool    DuplicateImage=false;

   if (pDevice)
      if (pDevice->Duplicate)
         DuplicateImage = true;

   if (CONFIG (CommandGetAddStateInfo)[0])
      LabelParams = tr("Additional state info") + "\n";

   LabelParams +=        tr("Size")
               +  "\n" + tr("Sector size")
               +  "\n" + tr("Image file");  if (DuplicateImage) LabelParams += "\n";
   LabelParams += "\n" + tr("Info file" );  if (DuplicateImage) LabelParams += "\n";
   LabelParams += "\n" + tr("Current speed")
               +  "\n" + tr("Started", "Start timestamp and running time")
               +  "\n" + tr("Hash calculation")
               +  "\n" + tr("Source verification")
               +  "\n" + tr("Image verification")
               +  "\n" + tr("Overall speed (all acquisitions)");
   pOwn->pLabelParams->setText (LabelParams);

   if (pDevice)
   {
      // Additional state info
      // ---------------------
      if (CONFIG (CommandGetAddStateInfo)[0])
         StrValue = pDevice->AddStateInfo.Info + "\n";

      // Size
      // ----
      Size = t_Device::GetSize (pDevice).toULongLong();
      StrValue += MainGetpNumberLocale()->toString(Size) + " " + tr("bytes");
      StrValue += " ("  + t_Device::GetSizeHumanFrac (pDevice, false).toString();
      StrValue += " / " + t_Device::GetSizeHumanFrac (pDevice, true ).toString() + ")";

      // Sector size
      // -----------
      quint64 SectorSize     = t_Device::GetSectorSize    (pDevice).toULongLong();
      quint64 SectorSizePhys = t_Device::GetSectorSizePhys(pDevice).toULongLong();
      StrValue += "\n" + MainGetpNumberLocale()->toString(SectorSize);
      if (SectorSize != SectorSizePhys)
         StrValue += " / " + MainGetpNumberLocale()->toString(SectorSizePhys);

      // Image/Info file
      // ---------------
      QString ImagePaths;
      QString InfoPaths;

      if (pDevice->Acquisition1.Format != t_File::NotSet)
      {
         CHK_EXIT (t_File::GetFormatExtension   (pDevice->Acquisition1.Format, pDevice->Acquisition1.Clone, 0, nullptr, &Format))
         ImagePaths = pDevice->Acquisition1.ImagePath + pDevice->Acquisition1.ImageFilename + QSTR_TO_PSZ(Format);
         InfoPaths  = pDevice->Acquisition1.InfoPath  + pDevice->Acquisition1.InfoFilename  + t_File::pExtensionInfo;
         if (DuplicateImage)
         {
            ImagePaths += "\n" + pDevice->Acquisition2.ImagePath + pDevice->Acquisition2.ImageFilename + QSTR_TO_PSZ(Format);
            InfoPaths  += "\n" + pDevice->Acquisition2.InfoPath  + pDevice->Acquisition2.InfoFilename  + t_File::pExtensionInfo;
         }
      }
      StrValue += "\n" + ImagePaths;
      StrValue += "\n" + InfoPaths;

      // Speed
      // -----
      StrSpeed = t_Device::GetCurrentSpeed  (pDevice).toString();
      if (!StrSpeed.isEmpty())
         StrSpeed += " MB/s";
      StrValue += "\n" + StrSpeed;

      // Running time
      // ------------
      if (pDevice->Acquisition1.Format == t_File::NotSet)
      {
         StrValue += "\n";
      }
      else
      {
         int Hours, Minutes, Seconds;

         StrValue += "\n" + pDevice->StartTimestamp.toString ("d. MMMM hh:mm:ss");

         if ((pDevice->GetState() == t_Device::Acquire      ) ||  // Don't display anything if no acquisition is running
             (pDevice->GetState() == t_Device::AcquirePaused) ||
             (pDevice->GetState() == t_Device::Launch       ) ||
             (pDevice->GetState() == t_Device::LaunchPaused ) ||
             (pDevice->GetState() == t_Device::Verify       ) ||
             (pDevice->GetState() == t_Device::VerifyPaused ))
              Seconds = pDevice->StartTimestamp.secsTo (QDateTime::currentDateTime());
         else Seconds = pDevice->StartTimestamp.secsTo (pDevice->StopTimestamp);
         Hours    = Seconds / SECONDS_PER_HOUR  ; Seconds -= Hours   * SECONDS_PER_HOUR;
         Minutes  = Seconds / SECONDS_PER_MINUTE; Seconds -= Minutes * SECONDS_PER_MINUTE;

         StrValue += QString(" (%1:%2:%3)") .arg(Hours,2,10,QChar('0')) .arg(Minutes,2,10,QChar('0')) .arg(Seconds,2,10,QChar('0'));
      }

      // Hash
      // ----
      StrValue += "\n";
      if (!pDevice->StartTimestamp.isNull())
         StrValue += t_InfoField::GetHashText (pDevice->Acquisition1.CalcMD5,
                                               pDevice->Acquisition1.CalcSHA1,
                                               pDevice->Acquisition1.CalcSHA256);
      // Source verification
      // -------------------
      StrValue += "\n";
      if (!pDevice->StartTimestamp.isNull())
      {
         if (pDevice->Acquisition1.VerifySrc)
              StrValue += tr("on" , "Display that verification is on");
         else StrValue += tr("off", "Display that verification is off");
      }
      // Image verification
      // ------------------
      StrValue += "\n";
      if (!pDevice->StartTimestamp.isNull())
      {
         if (pDevice->Acquisition1.VerifyDst)
              StrValue += tr("on" , "Display that verification is on");
         else StrValue += tr("off", "Display that verification is off");
         if (DuplicateImage)
         {
            StrValue += " | ";
            if (pDevice->Acquisition2.VerifyDst)
                 StrValue += tr("on" , "Display that verification is on");
            else StrValue += tr("off", "Display that verification is off");
         }
      }

      // Overall acquisition speed
      // -------------------------
      StrValue += "\n";
      QString Speed = pOwn->pDeviceList->GetOverallAcquisitionSpeed();
      if (!Speed.isNull())
         StrValue += Speed + " MB/s";
   }
   pOwn->pLabelValues->setText (StrValue);
}

