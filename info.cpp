// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Information about the acquisition, creates the info file
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

#include <errno.h>

#include <QtCore>

#include "common.h"

#include "qtutil.h"
#include "util.h"
#include "device.h"
#include "config.h"

static const char *pInfoDefaultColSep = "::";
static const char *pInfoDefaultColSepReplace = ":";


class t_InfoLocal
{
   public:
      t_pcAcquisition pAcquisition;
      QMutex           Mutex;
      QString          ColSep;
      QString          ColSepReplace;
      QStringList      TableRows;
};


t_Info::t_Info (t_pcAcquisition pAcquisition)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_INFO_FILE_CREATE_FAILED))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_INFO_FILE_OPEN_FAILED  ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_INFO_FILE_WRITE_FAILED ))
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_INFO_FILE_CLOSE_FAILED ))

      Initialised = true;
   }
   pOwn = new t_InfoLocal;
   pOwn->pAcquisition = pAcquisition;
   CHK_EXIT (InitTable (pInfoDefaultColSep, pInfoDefaultColSepReplace))
}

t_Info::~t_Info ()
{
   delete pOwn;
}

QString t_Info::FullFileName ()
{
   return pOwn->pAcquisition->InfoPath + pOwn->pAcquisition->InfoFilename + t_File::pExtensionInfo;
}


APIRET t_Info::Create (void)
{
   FILE *pFile;

   pOwn->Mutex.lock();
      pFile = fopen (QSTR_TO_PSZ(FullFileName()), "w");
      if (pFile == nullptr)
      {
          pOwn->Mutex.unlock();
          LOG_ERROR ("File %s cannot be opened. errno=%d '%s'", QSTR_TO_PSZ(FullFileName()), errno, ToolErrorTranslateErrno (errno))
          return ERROR_INFO_FILE_CREATE_FAILED;
      }
      if (UtilClose (pFile))
      {
          pOwn->Mutex.unlock();
          return ERROR_INFO_FILE_CLOSE_FAILED;
      }
   pOwn->Mutex.unlock();

   return NO_ERROR;
}

APIRET t_Info::vWrite (const char *pFormat, va_list pArguments)
{
   FILE *pFile;
   int    Ret;

   pOwn->Mutex.lock();
      pFile = fopen (QSTR_TO_PSZ(FullFileName()), "a");

      if (pFile == nullptr)
      {
          pOwn->Mutex.unlock();
          return ERROR_INFO_FILE_OPEN_FAILED;
      }

      Ret = vfprintf (pFile, pFormat, pArguments);
      if (Ret < 0)
      {
          pOwn->Mutex.unlock();
          return ERROR_INFO_FILE_WRITE_FAILED;
      }

      Ret = fclose (pFile);
   pOwn->Mutex.unlock();

   if (Ret < 0)
       return ERROR_INFO_FILE_CLOSE_FAILED;

   return NO_ERROR;
}

APIRET t_Info::Write (const char *pFormat, ...)
{
   va_list VaList;  //lint -esym(530,VaList) not initialised

   va_start (VaList, pFormat);
   CHK (vWrite (pFormat, VaList))
   va_end (VaList);

   return NO_ERROR;
}

APIRET t_Info::Write (const QString &Text)
{
   CHK (Write ("%s", QSTR_TO_PSZ(Text)))
   return NO_ERROR;
}

APIRET t_Info::WriteLn (const QString &Text)
{
   CHK (Write ("\r\n%s", QSTR_TO_PSZ(Text)))
   return NO_ERROR;
}

APIRET t_Info::Title (const QString &Text)
{
   int Len;

   CHK (WriteLn (Text))
   CHK (WriteLn ())
   Len = Text.length();
   for (int i=0; i<Len; i++)
      CHK (Write ("="))

   return NO_ERROR;
}

APIRET t_Info::WriteDeviceInfo (void)
{
   QString DeviceInfo;

   CHK (GetDeviceInfo (pOwn->pAcquisition->pDevice, false, DeviceInfo))

   CHK (Write (DeviceInfo))

   return NO_ERROR;
}

APIRET t_Info::GetDeviceInfo (t_pcDevice pDevice, bool RichText, QString &Info)
{
   QStringList *pDeviceInfoCommands;
   QString       Command;
   QString       Result;
   APIRET        rc;
   int           i;

   CHK (CfgGetDeviceInfoCommands (&pDeviceInfoCommands))
   for (i = 0; i < pDeviceInfoCommands->size(); i++)
   {
      Command = pDeviceInfoCommands->at(i);

      Command.replace ("%dev", pDevice->LinuxDevice, Qt::CaseInsensitive);
      if (i>0)
         Info += "\r\n\r\n";
      if (RichText)
         Info += "<b>";
      Info += tr("Command executed: %1") .arg(Command);

      rc = QtUtilProcessCommand (Command, &Result);
      if (rc == ERROR_QTUTIL_COMMAND_TIMEOUT)
      {
         Info += "\r\n" + tr("No information can be displayed as a timeout occurred while executing the command.");
         if (RichText)
            Info += "</b>";
      }
      else
      {
         CHK(rc)
         Info += "\r\n" + tr("Information returned:");
         if (RichText)
              Info += "</b>";
         else Info += "\r\n----------------------------------------------------------------------------------------------------";
         Result.replace ("\n\r", "\n");      // Make sure that every new line of the result
         Result.replace ("\r\n", "\n");      // has a CRLF (for Wind compatibility) and that
         Result.replace ("\n", "\r\n   ");   // the result lines are indented by 3 spaces.
         Result = Result.trimmed();
         Info += "\r\n   "  + Result;
         CHK (rc)
      }
   }

   return NO_ERROR;
}

APIRET t_Info::InitTable (const QString &ColSep, const QString &ColSepReplace)
{
   pOwn->ColSep        = ColSep;
   pOwn->ColSepReplace = ColSepReplace;
   pOwn->TableRows.clear();

   return NO_ERROR;
}

APIRET t_Info::AddRow (const QString &Row)
{
   pOwn->TableRows.append (Row);

   return NO_ERROR;
}

APIRET t_Info::WriteTable (void)
{
   QStringList ColList;
   QList<int>  ColWidth;
   int         r, c;

   // Calculate column widths
   // -----------------------
   for (r=0; r<pOwn->TableRows.count(); r++)
   {
      ColList = pOwn->TableRows[r].split (pOwn->ColSep);
      for (c=0; c<ColList.count(); c++)
      {
         if (c >= ColWidth.count())
            ColWidth.append (0);
         ColWidth[c] = GETMAX (ColWidth[c], ColList[c].length());
      }
   }

   // Write to info
   // -------------
   for (r=0; r<pOwn->TableRows.count(); r++)
   {
      CHK (WriteLn ())
      ColList = pOwn->TableRows[r].split (pOwn->ColSep);
      for (c=0; c<ColList.count(); c++)
      {
         CHK (Write (ColList[c].leftJustified (ColWidth[c])))
         if (c<ColList.count()-1)
            CHK (Write (pOwn->ColSepReplace))
      }
   }
   pOwn->TableRows.clear();

   return NO_ERROR;
}


