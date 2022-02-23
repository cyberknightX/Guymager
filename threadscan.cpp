// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Thread for scanning the connected devices
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

#include <QtCore>
#include <QtDBus/QtDBus>

#include <dlfcn.h>
#include <unistd.h>
#include <libudev.h>

#include "toolconstants.h"

#include "common.h"
#include "config.h"
#include "device.h"
#include "qtutil.h"
#include "media.h"
#include "dlgmessage.h"
#include "threadscan.h"

// --------------------------
//         Constants
// --------------------------

//#define DEBUG_SERNR

#ifdef DEBUG_SERNR
   #warning "DEBUG_SERNR must be switched off for final release"
#endif

const int THREADSCAN_WAIT_MAX         = 30000;
const int THREADSCAN_WAIT_GRANULARITY = 100;

const QString ThreadScanLibSearchDirs     = "/lib,/usr/lib,/usr/lib64,/usr/local/lib";  // Separate directories by commas
const int     ThreadScanLibSearchMaxDepth = 1;                                          // Max. depth for recursing into directories indicated under
                                                                                        // ThreadScanLibPartedSearchDirs, 0=only directory indicated, 1=also descend 1 below, ...
const QString ThreadScanLibPartedSearchPattern = "libparted*.so*";
const QString ThreadScanLibUdevSearchPattern   = "libudev*.so*";

// -----------------------------------
//        Worker base class
// -----------------------------------

t_ThreadScanWorker::t_ThreadScanWorker (t_ThreadScan *pThreadScan)
{
   poThreadScan = pThreadScan;
}

// -----------------------------------
//  Utility functions used by workers
// -----------------------------------


static bool ThreadScanDeviceIsListed (QStringList *pList, t_pDevice pDevice)
{
   bool Listed=false;

   if (CONFIG(LocalHiddenDevicesUseRegExp))
   {
      QRegExp RegExp;

      RegExp.setCaseSensitivity(Qt::CaseInsensitive);
      foreach (QString Str, *pList)
      {
         RegExp.setPattern (Str);
         Listed = (RegExp.indexIn(pDevice->SerialNumber) != -1) ||
                  (RegExp.indexIn(pDevice->LinuxDevice ) != -1) ||
                  (RegExp.indexIn(pDevice->Model       ) != -1) ||
                  (RegExp.indexIn(pDevice->NativePath  ) != -1) ||
                  (RegExp.indexIn(pDevice->ByPath      ) != -1) ||
                  (RegExp.indexIn(pDevice->Interface   ) != -1);
         if (Listed)
            break;
      }
   }
   else
   {
      Listed = pList->contains (pDevice->SerialNumber, Qt::CaseInsensitive) ||
               pList->contains (pDevice->LinuxDevice , Qt::CaseInsensitive) ||
               pList->contains (pDevice->Model       , Qt::CaseInsensitive) ||
               pList->contains (pDevice->NativePath  , Qt::CaseInsensitive) ||
               pList->contains (pDevice->ByPath      , Qt::CaseInsensitive) ||
               pList->contains (pDevice->Interface   , Qt::CaseInsensitive);
   }
   return Listed;
}

static APIRET ThreadScanSetLocal (t_pDevice pDevice)
{
   QStringList *pLocalDevices;

   CHK_EXIT (CfgGetLocalDevices (&pLocalDevices))
   pDevice->Local = ThreadScanDeviceIsListed (pLocalDevices, pDevice);

   return NO_ERROR;
}

static bool ThreadScanIsHidden (t_pDevice pDevice)
{
   QStringList *pHiddenDevices;
   bool          Hidden;

   CHK_EXIT (CfgGetHiddenDevices (&pHiddenDevices))
   Hidden = ThreadScanDeviceIsListed (pHiddenDevices, pDevice);
   return Hidden;
}

static APIRET ThreadScanGetAddStateInfo (t_pDevice pDevice)
{
   QString     Command;
   APIRET      rc;
   QString     StateInfo;
   QStringList StateInfoList;

   Command = CONFIG (CommandGetAddStateInfo);
   if (!Command.isEmpty())
   {
      Command.replace ("%dev"  , pDevice->LinuxDevice    , Qt::CaseInsensitive);
      Command.replace ("%local", pDevice->Local ? "1":"0", Qt::CaseInsensitive);
      rc = QtUtilProcessCommand (Command, &StateInfo);
      if (rc == ERROR_QTUTIL_COMMAND_TIMEOUT)
      {
         LOG_INFO ("Timeout while trying to get additional state information for %s, additional state info will be ignored", QSTR_TO_PSZ(pDevice->LinuxDevice))
         return NO_ERROR;
      }
      CHK(rc)
      StateInfoList = StateInfo.split("\n");
      if (StateInfoList.count() > 0) pDevice->AddStateInfo.Info          =  StateInfoList[0].trimmed();
      if (StateInfoList.count() > 1) pDevice->AddStateInfo.CanBeAcquired = (StateInfoList[1].trimmed() != "0");
      if (StateInfoList.count() > 2) pDevice->AddStateInfo.Color         =  StateInfoList[2].trimmed().toInt();
   }
   return NO_ERROR;
}

// --------------------------
//  t_ThreadScanWorkerParted
// --------------------------

// Scan the devices using libparted and bash command to gather device info

typedef  void       (*t_pFnLibPartedProbeAll) (void) ;
typedef  PedDevice* (*t_pFnLibPartedGetNext ) (const PedDevice* pDev);
typedef  void       (*t_pFnLibPartedFreeAll ) (void);

class t_ThreadScanWorkerPartedLocal
{
   public:
      t_ThreadScanWorkerPartedLocal ()
         :LibPartedLoaded      (false), pLibPartedHandle (nullptr),
          pFnLibPartedProbeAll (nullptr) ,
          pFnLibPartedGetNext  (nullptr) ,
          pFnLibPartedFreeAll  (nullptr)
      {
      }

   public:
      bool         LibPartedLoaded;
      void       *pLibPartedHandle;
      t_pFnLibPartedProbeAll pFnLibPartedProbeAll;
      t_pFnLibPartedGetNext  pFnLibPartedGetNext;
      t_pFnLibPartedFreeAll  pFnLibPartedFreeAll;
};


static APIRET ThreadScanGetSerialNumber (char const *pcDeviceName, QString &SerialNumber)
{
   QString Command;
   APIRET  rc;

   Command = CONFIG (CommandGetSerialNumber);
   Command.replace ("%dev", pcDeviceName, Qt::CaseInsensitive);

   rc = QtUtilProcessCommand (Command, &SerialNumber);
   if (rc == ERROR_QTUTIL_COMMAND_TIMEOUT)
      return rc;
   CHK(rc)
   SerialNumber = SerialNumber.trimmed();

   return NO_ERROR;
}

static APIRET ThreadScanCheckForceSerialNumber (char const *pcDeviceName, QVariant *pSerialNumber)
{
   if (CONFIG(ForceCommandGetSerialNumber))
   {
      QString SN;
      APIRET  rc;

      rc = ThreadScanGetSerialNumber (pcDeviceName, SN);
      if (rc == ERROR_QTUTIL_COMMAND_TIMEOUT)
      {
         LOG_INFO ("Timeout while trying to get serial number for %s", pcDeviceName)
         SN.clear();
      }
      else
      {
         CHK_EXIT (rc)
      }
      pSerialNumber->setValue(SN);
   }

   return NO_ERROR;
}

static APIRET ThreadScanFindLib0 (const QString &Path, QStringList &Files, const QString &SearchPattern, int RecurseLevel=0)
{
   QStringList    SearchDirs;
   QFileInfoList  ItemList;
//   static int DirCount=0;

   // Search for files in current dir
   // -------------------------------
//   printf ("%d / %d - Into %s\n", RecurseLevel, DirCount++, QSTR_TO_PSZ(Path));
   QDir SearchDir(Path);
   ItemList = SearchDir.entryInfoList (QStringList(SearchPattern), QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Name);
   foreach (const QFileInfo &Item, ItemList)
   {
      if (Item.isDir())
      {
         if (RecurseLevel<ThreadScanLibSearchMaxDepth)
         {
            CHK (ThreadScanFindLib0 (Item.absoluteFilePath(), Files, SearchPattern, ++RecurseLevel))  // Recurse into subdirs (must be pre-increment!)
            RecurseLevel--;
         }
      }
      else
      {
         LOG_INFO ("lib for '%s' found: %s", QSTR_TO_PSZ(SearchPattern), QSTR_TO_PSZ(Item.absoluteFilePath()));
         Files += Item.absoluteFilePath();
      }
   }

   return NO_ERROR;
}


static APIRET ThreadScanFindLib (QStringList &Files, const QString &SearchPattern)
{
   QStringList SearchDirs;
   int         i;

   SearchDirs = ThreadScanLibSearchDirs.split(",");

   for (i=0; i<SearchDirs.size(); i++)
      CHK (ThreadScanFindLib0 (SearchDirs[i], Files, SearchPattern))

   if (Files.empty())
      LOG_INFO ("No libparted could be found")

   return NO_ERROR;
}

t_ThreadScanWorkerParted::t_ThreadScanWorkerParted (t_ThreadScan *pThreadScan, APIRET &rc)
   :t_ThreadScanWorker (pThreadScan)
{
   QStringList   FilenameList;
   QString       Filename;
   char        *pErrStr;
   int           i;
   bool          Found = false;

   rc   = ERROR_THREADSCAN_LIBPARTED_NOTWORKING;
   pOwn = new t_ThreadScanWorkerPartedLocal();
   CHK_EXIT (ThreadScanFindLib (FilenameList, ThreadScanLibPartedSearchPattern))

   for (i=0; (i<FilenameList.size()) && (!Found); i++)
   {
      Filename = FilenameList[i];
      pOwn->pLibPartedHandle = dlopen (QSTR_TO_PSZ(Filename), RTLD_NOW);
      if (pOwn->pLibPartedHandle == nullptr)
      {
         LOG_INFO ("Trying %s -- dlopen returns (%s)", QSTR_TO_PSZ(Filename), dlerror())
         continue;
      }
      LOG_INFO ("Trying %s -- dlopen ok", QSTR_TO_PSZ(Filename))

      // GETFN: See dlsym documentation concerning the usage of dlerror for error checking
      #define GETFN(pFn, Type, pFnName)                                                    \
         dlerror();                                                                        \
         pFn = (Type)dlsym (pOwn->pLibPartedHandle, pFnName);                              \
         pErrStr = dlerror();                                                              \
         if (pErrStr)                                                                      \
         {                                                                                 \
            LOG_INFO ("Trying %s -- dlsym for %s returns (%s)", QSTR_TO_PSZ(Filename), pFnName, dlerror()); \
            dlclose (pOwn->pLibPartedHandle);                                              \
            pOwn->pLibPartedHandle    = nullptr;                                           \
            pOwn->pFnLibPartedProbeAll= nullptr;                                           \
            pOwn->pFnLibPartedGetNext = nullptr;                                           \
            pOwn->pFnLibPartedFreeAll = nullptr;                                           \
            continue;                                                                      \
         }                                                                                 \
         else                                                                              \
         {                                                                                 \
            LOG_INFO ("Trying %s -- dlsym for %s ok", QSTR_TO_PSZ(Filename), pFnName);     \
         }

      GETFN (pOwn->pFnLibPartedProbeAll, t_pFnLibPartedProbeAll, "ped_device_probe_all")
      GETFN (pOwn->pFnLibPartedGetNext , t_pFnLibPartedGetNext , "ped_device_get_next" )
      GETFN (pOwn->pFnLibPartedFreeAll , t_pFnLibPartedFreeAll , "ped_device_free_all" )
      #undef GETFN

      Found = true;
   }

   if (Found)
      rc = NO_ERROR;
}

t_ThreadScanWorkerParted::~t_ThreadScanWorkerParted (void)
{
   int rc;

   if (pOwn->pLibPartedHandle)
   {
      rc = dlclose (pOwn->pLibPartedHandle);
      if (rc)
         LOG_INFO ("dlclose returned %d (%s).", rc, dlerror());
   }

   delete pOwn;
}


void t_ThreadScanWorkerParted::SlotRescan (void)
{
   t_pDeviceList  pDeviceList;
   t_pDevice      pDevice;
   PedDevice     *pPedDev;
   QString         SerialNumber;
   APIRET          rc;

   emit (SignalScanStarted ());
   LOG_INFO ("Rescanning devices")
   CHK_EXIT (poThreadScan->SetDebugMessage ("SlotRescan called (parted)"))

   pOwn->pFnLibPartedProbeAll ();

   pDeviceList = new t_DeviceList;
   pPedDev     = nullptr;
   while ((pPedDev = pOwn->pFnLibPartedGetNext (pPedDev)))  //lint !e820
   {
      rc = ThreadScanGetSerialNumber (pPedDev->path, SerialNumber);
      if (rc == ERROR_QTUTIL_COMMAND_TIMEOUT)
      {
         LOG_INFO ("Device scan aborted due to timeout while trying to get serial number for %s", pPedDev->path)
         pOwn->pFnLibPartedFreeAll ();
         delete pDeviceList;
         return;
      }
      #ifdef DEBUG_SERNR
           printf ("\n----- libparted device info ---------");
           printf ("\nPath:             [%s]", pPedDev->path );
           printf ("\nModel:            [%s]", pPedDev->model);
           printf ("\nSectorSize:       %lld", pPedDev->sector_size);
           printf ("\nSectorSizePhys:   %lld", pPedDev->phys_sector_size);
           printf ("\nLength (Sectors): %lld", pPedDev->length);
           printf ("\nSerial number:    [%s]   -- read out by command %s"  , QSTR_TO_PSZ(SerialNumber), CONFIG (CommandGetSerialNumber));
           printf ("\n");
      #endif


      pDevice = new t_Device (SerialNumber, pPedDev);
      if (ThreadScanIsHidden(pDevice))
      {
         delete pDevice;
      }
      else
      {
         pDeviceList->append (pDevice);
         if (pDevice->LinuxDevice.startsWith ("/dev/fd",  Qt::CaseSensitive)) // Not a good way for checking this, but I don't know how to extract the information from
            pDevice->Removable = true;                                        // PedDevice. BTW, this won't work with other removable devices, for instance memory sticks.
         CHK_EXIT (ThreadScanSetLocal        (pDevice));
         CHK_EXIT (ThreadScanGetAddStateInfo (pDevice));
      }
   }
   pOwn->pFnLibPartedFreeAll ();

   emit (SignalScanFinished (pDeviceList));
   CHK_EXIT (poThreadScan->SetDebugMessage ("Leaving SlotRescan (parted)"))
}

// ------------------------
//  t_ThreadScanWorkerUdev
// ------------------------

#define THREADSCAN_UDEV_PATH_RAM  "/sys/devices/virtual/block/ram"
#define THREADSCAN_UDEV_PATH_LOOP "/sys/devices/virtual/block/loop"

typedef struct udev *            (*t_pFnLibUdev_udev_new)                           (void);
typedef struct udev *            (*t_pFnLibUdev_udev_unref)                         (struct udev *udev);
typedef void                     (*t_pFnLibUdev_udev_device_unref)                  (struct udev_device *udev_device);
typedef struct udev_enumerate *  (*t_pFnLibUdev_udev_enumerate_new)                 (struct udev *udev);
typedef struct udev_enumerate *  (*t_pFnLibUdev_udev_enumerate_unref)               (struct udev_enumerate *udev_enumerate);
typedef int                      (*t_pFnLibUdev_udev_enumerate_add_match_subsystem) (struct udev_enumerate *udev_enumerate, const char *subsystem);
typedef int                      (*t_pFnLibUdev_udev_enumerate_add_match_property)  (struct udev_enumerate *udev_enumerate, const char *property, const char *value);
typedef int                      (*t_pFnLibUdev_udev_enumerate_scan_devices)        (struct udev_enumerate *udev_enumerate);
typedef struct udev_list_entry * (*t_pFnLibUdev_udev_enumerate_get_list_entry)      (struct udev_enumerate *udev_enumerate);
typedef const char *             (*t_pFnLibUdev_udev_list_entry_get_name)           (struct udev_list_entry *list_entry);
typedef struct udev_device *     (*t_pFnLibUdev_udev_device_new_from_syspath)       (struct udev *udev, const char *syspath);
typedef const char *             (*t_pFnLibUdev_udev_device_get_property_value)     (struct udev_device *udev_device, const char *key);
typedef struct udev_list_entry * (*t_pFnLibUdev_udev_list_entry_get_next)           (struct udev_list_entry *list_entry);

#define LibUdevListEntryForEach(list_entry, first_entry) \
   for (list_entry = first_entry;                        \
        list_entry != nullptr;                              \
        list_entry = pOwn->pFnLibUdev_udev_list_entry_get_next(list_entry))

#pragma GCC diagnostic ignored "-Wunused-result"

class t_ThreadScanWorkerUdevLocal
{
   public:
      t_ThreadScanWorkerUdevLocal ()
         :LibUdevLoaded  (false),
          pLibUdevHandle (nullptr)
      {
         ClearFnPointers();
      }

      void ClearFnPointers (void)
      {
         pFnLibUdev_udev_new                           = nullptr;
         pFnLibUdev_udev_unref                         = nullptr;
         pFnLibUdev_udev_device_unref                  = nullptr;
         pFnLibUdev_udev_enumerate_new                 = nullptr;
         pFnLibUdev_udev_enumerate_unref               = nullptr;
         pFnLibUdev_udev_enumerate_add_match_subsystem = nullptr;
         pFnLibUdev_udev_enumerate_add_match_property  = nullptr;
         pFnLibUdev_udev_enumerate_scan_devices        = nullptr;
         pFnLibUdev_udev_enumerate_get_list_entry      = nullptr;
         pFnLibUdev_udev_list_entry_get_name           = nullptr;
         pFnLibUdev_udev_device_new_from_syspath       = nullptr;
         pFnLibUdev_udev_device_get_property_value     = nullptr;
         pFnLibUdev_udev_list_entry_get_next           = nullptr;
      }

      // GetFileContents; Join both path parts and read file contents into ppData.
      // ppData is allocated by GetFileContents and must be freed by caller
      // (only in case of successful call).
      // Return values: 0=ok, 1=Error.
      int GetFileContents (const char *pPath, const char *pSubPath, char **ppData)
      {
         const size_t   MaxSize  = 65536; // If the file is bigger than this then there's surely something wrong
         char         *pFullPath = nullptr;
         FILE         *pFile     = nullptr;
         size_t         Read;
         size_t         Size;

         *ppData = nullptr;

         // Open file
         // ---------
         (void) asprintf (&pFullPath, "%s/%s", pPath, pSubPath);
         if (!pFullPath)
            goto Err;

         pFile = fopen(pFullPath, "rb");
         if (!pFile)
            goto Err;

         // Get and check file size
         // -----------------------
         if (fseek (pFile, 0, SEEK_END))
            goto Err;

         Size = ftell (pFile);
         if (Size >= MaxSize)
            goto Err;

         if (fseek (pFile, 0, SEEK_SET))
            goto Err;

         // Read data
         // ---------
         *ppData = (char *) malloc(Size);
         if (!*ppData)
            goto Err;

         Read = fread (*ppData, 1, Size, pFile);
         if ((Read==0) || (Read >= MaxSize))
            goto Err;
         (*ppData)[Read-1] = '\0';

         // Free resources
         // --------------
         fclose (pFile);
         free   (pFullPath);
         return 0;

      Err:
         if (pFullPath) free   (pFullPath);
         if (*ppData)   free   (*ppData);
         if (pFile)     fclose (pFile);
         return 1;
      }

      // GetFileContentsNum: See GetFileContents.
      // Expects the file to start with a numerical value. If not, an error is returned.
      // Same error codes as GetFileContents.
      int GetFileContentsNum (const char *pPath, const char *pSubPath, long long *pVal)
      {
         char *pData;
         char *pTail;
         int    rc;

         rc = GetFileContents (pPath, pSubPath, &pData);
         if (rc)
            return rc;

         *pVal = strtoll (pData, &pTail, 10);
         free (pData);
         if (pTail == pData) // Nothing converted?
            return 1;

         return 0;
      }

      int GetUdevDevicePropertyNum (struct udev_device *pUdevDevice, const char *pProperty, long long *pVal)
      {
         const char *pStr;
         char       *pTail;

         pStr = pFnLibUdev_udev_device_get_property_value (pUdevDevice, pProperty);
         if (!pStr)
            return 1;

         *pVal = strtoll (pStr, &pTail, 10);
         if (pTail == pStr) // Nothing converted?
            return 1;

         return 0;
      }

   public:
      bool   LibUdevLoaded;
      void *pLibUdevHandle;
      t_pFnLibUdev_udev_new                           pFnLibUdev_udev_new;
      t_pFnLibUdev_udev_unref                         pFnLibUdev_udev_unref;
      t_pFnLibUdev_udev_device_unref                  pFnLibUdev_udev_device_unref;
      t_pFnLibUdev_udev_enumerate_new                 pFnLibUdev_udev_enumerate_new;
      t_pFnLibUdev_udev_enumerate_unref               pFnLibUdev_udev_enumerate_unref;
      t_pFnLibUdev_udev_enumerate_add_match_subsystem pFnLibUdev_udev_enumerate_add_match_subsystem;
      t_pFnLibUdev_udev_enumerate_add_match_property  pFnLibUdev_udev_enumerate_add_match_property;
      t_pFnLibUdev_udev_enumerate_scan_devices        pFnLibUdev_udev_enumerate_scan_devices;
      t_pFnLibUdev_udev_enumerate_get_list_entry      pFnLibUdev_udev_enumerate_get_list_entry;
      t_pFnLibUdev_udev_list_entry_get_name           pFnLibUdev_udev_list_entry_get_name;
      t_pFnLibUdev_udev_device_new_from_syspath       pFnLibUdev_udev_device_new_from_syspath;
      t_pFnLibUdev_udev_device_get_property_value     pFnLibUdev_udev_device_get_property_value;
      t_pFnLibUdev_udev_list_entry_get_next           pFnLibUdev_udev_list_entry_get_next;
};

t_ThreadScanWorkerUdev::t_ThreadScanWorkerUdev (t_ThreadScan *pThreadScan, APIRET &rc)
   :t_ThreadScanWorker (pThreadScan)
{
   QStringList   FilenameList;
   QString       Filename;
   char        *pErrStr;
   int           i;
   bool          Found = false;

   rc   = ERROR_THREADSCAN_LIBUDEV_NOTWORKING;
   pOwn = new t_ThreadScanWorkerUdevLocal();
   CHK_EXIT (ThreadScanFindLib (FilenameList, ThreadScanLibUdevSearchPattern))

   for (i=0; (i<FilenameList.size()) && (!Found); i++)
   {
      Filename = FilenameList[i];
      pOwn->pLibUdevHandle = dlopen (QSTR_TO_PSZ(Filename), RTLD_NOW);
      if (pOwn->pLibUdevHandle == nullptr)
      {
         LOG_INFO ("Trying %s -- dlopen returns (%s)", QSTR_TO_PSZ(Filename), dlerror())
         continue;
      }
      LOG_INFO ("Trying %s -- dlopen ok", QSTR_TO_PSZ(Filename))

      // GETFN: See dlsym documentation concerning the usage of dlerror for error checking
      #define GETFN(pFn, Type, pFnName)                                                                     \
         dlerror();                                                                                         \
         pFn = (Type)dlsym (pOwn->pLibUdevHandle, pFnName);                                                 \
         pErrStr = dlerror();                                                                               \
         if (pErrStr)                                                                                       \
         {                                                                                                  \
            LOG_INFO ("Trying %s -- dlsym for %s returns (%s)", QSTR_TO_PSZ(Filename), pFnName, dlerror()); \
            dlclose (pOwn->pLibUdevHandle);                                                                 \
            pOwn->pLibUdevHandle = nullptr;                                                                    \
            pOwn->ClearFnPointers();                                                                        \
            continue;                                                                                       \
         }                                                                                                  \
         else                                                                                               \
         {                                                                                                  \
            LOG_INFO ("Trying %s -- dlsym for %s ok", QSTR_TO_PSZ(Filename), pFnName);                      \
         }

      GETFN (pOwn->pFnLibUdev_udev_new                          , t_pFnLibUdev_udev_new                          , "udev_new")
      GETFN (pOwn->pFnLibUdev_udev_unref                        , t_pFnLibUdev_udev_unref                        , "udev_unref")
      GETFN (pOwn->pFnLibUdev_udev_device_unref                 , t_pFnLibUdev_udev_device_unref                 , "udev_device_unref")
      GETFN (pOwn->pFnLibUdev_udev_enumerate_new                , t_pFnLibUdev_udev_enumerate_new                , "udev_enumerate_new")
      GETFN (pOwn->pFnLibUdev_udev_enumerate_unref              , t_pFnLibUdev_udev_enumerate_unref              , "udev_enumerate_unref")
      GETFN (pOwn->pFnLibUdev_udev_enumerate_add_match_subsystem, t_pFnLibUdev_udev_enumerate_add_match_subsystem, "udev_enumerate_add_match_subsystem")
      GETFN (pOwn->pFnLibUdev_udev_enumerate_add_match_property , t_pFnLibUdev_udev_enumerate_add_match_property , "udev_enumerate_add_match_property")
      GETFN (pOwn->pFnLibUdev_udev_enumerate_scan_devices       , t_pFnLibUdev_udev_enumerate_scan_devices       , "udev_enumerate_scan_devices")
      GETFN (pOwn->pFnLibUdev_udev_enumerate_get_list_entry     , t_pFnLibUdev_udev_enumerate_get_list_entry     , "udev_enumerate_get_list_entry")
      GETFN (pOwn->pFnLibUdev_udev_list_entry_get_name          , t_pFnLibUdev_udev_list_entry_get_name          , "udev_list_entry_get_name")
      GETFN (pOwn->pFnLibUdev_udev_device_new_from_syspath      , t_pFnLibUdev_udev_device_new_from_syspath      , "udev_device_new_from_syspath")
      GETFN (pOwn->pFnLibUdev_udev_device_get_property_value    , t_pFnLibUdev_udev_device_get_property_value    , "udev_device_get_property_value")
      GETFN (pOwn->pFnLibUdev_udev_list_entry_get_next          , t_pFnLibUdev_udev_list_entry_get_next          , "udev_list_entry_get_next")
      #undef GETFN

      Found = true;
   }

   if (Found)
      rc = NO_ERROR;
}

t_ThreadScanWorkerUdev::~t_ThreadScanWorkerUdev (void)
{
   int rc;

   if (pOwn->pLibUdevHandle)
   {
      rc = dlclose (pOwn->pLibUdevHandle);
      if (rc)
         LOG_INFO ("dlclose returned %d (%s).", rc, dlerror());
   }

   delete pOwn;
}

#define CHK_UDEV(x)                                \
{                                                  \
   int rc = (x);                                   \
   if (rc<0)                                       \
   {                                               \
      LOG_ERROR ("Error in udev call (rc=%d), aborting device scan",rc); \
      goto End;                                    \
   }                                               \
}


void t_ThreadScanWorkerUdev::SlotRescan (void)
{
   t_pDeviceList  pDeviceList;
   t_pDevice      pDevice;

   struct udev            *pUdev       = nullptr;
   struct udev_enumerate  *pUdevEnum   = nullptr;
   struct udev_device     *pUdevDevice = nullptr;
   struct udev_list_entry *pUdevList;
   struct udev_list_entry *pUdevEntry;
   const char             *pPath;
   const char             *pLinuxDevice;
   const char             *pSerial;
   const char             *pVendor;
   const char             *pModel;
   const char             *pRaidUuid;
   const char             *pRaidLevel;
   char                   *pLoopBacking = nullptr;
   int                      rc;
   long long int            IsRemovable;
   long long int            IsCD;
   long long int            HasMedia;
   long long int            SectorSize;
   long long int            Size;
   QString                  Model;

   emit (SignalScanStarted ());
   LOG_INFO ("Rescanning devices")
   CHK_EXIT (poThreadScan->SetDebugMessage ("SlotRescan called (udev)"))

   pDeviceList = new t_DeviceList;

   pUdev = pOwn->pFnLibUdev_udev_new();
   if (!pUdev)
      goto End;
   pUdevEnum = pOwn->pFnLibUdev_udev_enumerate_new (pUdev);
   if (!pUdevEnum)
      goto End;

   CHK_UDEV (pOwn->pFnLibUdev_udev_enumerate_add_match_subsystem (pUdevEnum, "block"))
   CHK_UDEV (pOwn->pFnLibUdev_udev_enumerate_add_match_property  (pUdevEnum, "DEVTYPE", "disk"))
   CHK_UDEV (pOwn->pFnLibUdev_udev_enumerate_scan_devices        (pUdevEnum))

   pUdevList = pOwn->pFnLibUdev_udev_enumerate_get_list_entry (pUdevEnum);
   if (pUdevList == nullptr)
   {
      LOG_ERROR ("No disk block devices found.");
      goto End;
   }

   // Loop trough result set
   LibUdevListEntryForEach (pUdevEntry, pUdevList)
   {
      pPath = pOwn->pFnLibUdev_udev_list_entry_get_name (pUdevEntry);
      if (!pPath)
         continue;

      if (pUdevDevice)
         pOwn->pFnLibUdev_udev_device_unref (pUdevDevice);

      pUdevDevice = pOwn->pFnLibUdev_udev_device_new_from_syspath (pUdev, pPath);
      if (!pUdevDevice)
         continue;

      if (strncmp (pPath, THREADSCAN_UDEV_PATH_RAM, strlen (THREADSCAN_UDEV_PATH_RAM)) == 0)   // Ignore RAM devices
         continue;

      pLinuxDevice = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "DEVNAME"); // Ignore devices without standard Linux /dev/xxx path
      if (!pLinuxDevice)
         continue;

      pVendor   = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "ID_VENDOR"      );
      pModel    = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "ID_MODEL"       );
      pRaidUuid = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "MD_UUID"        );
      pSerial   = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "ID_SERIAL_SHORT");
      if (!pSerial)
         pSerial = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "ID_SERIAL");
      if (!pSerial)
         pSerial = pRaidUuid; // Take UUID for md arrays (if it exists)

      if (strncmp (pPath, THREADSCAN_UDEV_PATH_LOOP, strlen (THREADSCAN_UDEV_PATH_LOOP)) == 0) // Ignore non-mounted loop devices
      {                                                                          // Gather extra info for mounted loop devices
         rc = pOwn->GetFileContents (pPath, "loop/backing_file", &pLoopBacking);
         if (rc)
            continue;
         pModel = basename (pLoopBacking);  // Take mounted filename as model
      }

      HasMedia=1;
      if (pOwn->GetFileContentsNum (pPath, "removable", &IsRemovable) == 0)       // Check drives with removable media, ignore those where no media is inserted.
      {                                                                           // If it's a CD/DVD/BR then there should be property ID_CDROM_MEDIA to check if
         if (IsRemovable)                                                         // a disc is inserted. This is not the case for floppy drives. Floppy drive do have
         {                                                                        // property ID_FLOPPY but there is not property for telling us if a floppy disc is
            rc = pOwn->GetUdevDevicePropertyNum (pUdevDevice, "ID_CDROM", &IsCD); // inserted or not. However, the size switches to 0 if the drive is empty.
            if ((rc==0) && IsCD)                                                  // Tested with external USB floppy drive "SmartDisk FDUSB-B2"
            {
               HasMedia=0;
               rc = pOwn->GetUdevDevicePropertyNum (pUdevDevice, "ID_CDROM_MEDIA", &HasMedia);
            }
         }
      }
      else
      {
         IsRemovable=0;
      }

      if (pOwn->GetFileContentsNum (pPath, "queue/hw_sector_size", &SectorSize))
         SectorSize = 512;  // not always available, set to 512 in that case

      if (pOwn->GetFileContentsNum (pPath, "size", &Size)) // An alternative would be to get sysattr 'size',
         Size = 0;                                         // but this is the way it is done by udisks

      Size *= 512; // Do not multiply by SectorSize, multiply by 512!

      if (HasMedia && Size)
      {
         Model.clear();
         if (pModel)
         {
            Model = pModel;
         }
         else
         {
            if (pRaidUuid)
            {
               pRaidLevel = pOwn->pFnLibUdev_udev_device_get_property_value (pUdevDevice, "MD_LEVEL");
               Model = QString ("Linux SW ") + (pRaidLevel ? pRaidLevel : "raid");
            }
         }
         if (!Model.contains (pVendor, Qt::CaseInsensitive))
            Model = QString(pVendor) + " " + Model;

         pDevice = new t_Device (pSerial ? pSerial : QString(), pLinuxDevice, Model, SectorSize, SectorSize, Size);

         if (ThreadScanIsHidden(pDevice))
         {
            delete pDevice;
         }
         else
         {
            pDevice->Removable = IsRemovable ? true : false;

            if (CONFIG(QueryDeviceMediaInfo))
               CHK_EXIT (pDevice->MediaInfo.QueryDevice(pLinuxDevice))
            CHK_EXIT (ThreadScanSetLocal        (pDevice));
            CHK_EXIT (ThreadScanGetAddStateInfo (pDevice));

            pDeviceList->append (pDevice);
         }
      }
      if (pLoopBacking)
      {
         free (pLoopBacking);
         pLoopBacking = nullptr;
      }
   }

End:
   if (pUdevDevice)  pOwn->pFnLibUdev_udev_device_unref    (pUdevDevice);
   if (pUdevEnum)    pOwn->pFnLibUdev_udev_enumerate_unref (pUdevEnum);
   if (pUdev)        pOwn->pFnLibUdev_udev_unref           (pUdev);

   emit (SignalScanFinished (pDeviceList));
   CHK_EXIT (poThreadScan->SetDebugMessage ("Leaving SlotRescan (parted)"))
}

// --------------------------
//  t_ThreadScanWorkerHAL
// --------------------------

// Scan the devices using DBUS/HAL

#define HAL_SERVICE      "org.freedesktop.Hal"
#define HAL_MANAGER_PATH "/org/freedesktop/Hal/Manager"
#define HAL_MANAGER_IF   "org.freedesktop.Hal.Manager"
#define HAL_DEVICE_IF    "org.freedesktop.Hal.Device"

class t_ThreadScanWorkerHALLocal
{
   public:
      t_ThreadScanWorkerHALLocal (void)
      {
         pDBusConnection = new QDBusConnection (QDBusConnection::systemBus());
         pDBusInterface  = nullptr;
      }

     ~t_ThreadScanWorkerHALLocal ()
      {
         QDBusConnection::disconnectFromBus (pDBusConnection->baseService());
         delete pDBusConnection;
         pDBusConnection = nullptr;
         pDBusInterface  = nullptr;
      }

   public:
      QDBusConnection          *pDBusConnection;
      QDBusConnectionInterface *pDBusInterface;
};


t_ThreadScanWorkerHAL::t_ThreadScanWorkerHAL (t_ThreadScan *pThreadScan, APIRET &rc)
   :t_ThreadScanWorker (pThreadScan)
{
   pOwn = new t_ThreadScanWorkerHALLocal;
   if (!pOwn->pDBusConnection->isConnected())
   {
      LOG_INFO ("DBus connection failed")
      rc = ERROR_THREADSCAN_DBUSHAL_NOTWORKING;
      return;
   }

   pOwn->pDBusInterface = pOwn->pDBusConnection->interface();
   if (!pOwn->pDBusInterface->isServiceRegistered (HAL_SERVICE))
   {
      LOG_INFO ("HAL not registered")
      rc = ERROR_THREADSCAN_DBUSHAL_NOTWORKING;
      return;
   }
   rc = NO_ERROR;
}

t_ThreadScanWorkerHAL::~t_ThreadScanWorkerHAL (void)
{
   delete pOwn;
}

QList<QVariant> t_ThreadScanWorkerHAL::CallMethod (const QString &Device, const QString &Method, const QString &Argument)
{
   QList<QVariant> Args;
   QDBusMessage    Message = QDBusMessage::createMethodCall (HAL_SERVICE, Device, HAL_DEVICE_IF, Method);
   QDBusMessage    Reply;

   Args += Argument;
   Message.setArguments (Args);
   Reply = pOwn->pDBusConnection->call (Message);

   if (Reply.type() == QDBusMessage::ErrorMessage)
   {
      LOG_ERROR ("DBus returned '%s' for method %s on device %s", QSTR_TO_PSZ (Reply.errorName()),
                                                                  QSTR_TO_PSZ (Method),
                                                                  QSTR_TO_PSZ (Device))
      return QList<QVariant>(); // return empty list
   }

   return Reply.arguments();
}

QVariant t_ThreadScanWorkerHAL::CallMethodSingle (const QString &Device, const QString &Method, const QString &Argument)
{
   QList<QVariant> Results;

   Results = CallMethod (Device, Method, Argument);
   if (Results.first().isNull())
      return QVariant();

   return Results.first();
}

APIRET t_ThreadScanWorkerHAL::GetProperty (const QString &Device, const QString &Property, QList<QVariant> &VarList)
{
   QVariant Exists;

   Exists = CallMethodSingle (Device, "PropertyExists", Property);
   if (Exists.toBool())
        VarList = CallMethod (Device, "GetProperty", Property);
   else VarList = QList<QVariant>();  // Construct an empty, invalid list to show that the property doesn't exist

   return NO_ERROR;
}

APIRET t_ThreadScanWorkerHAL::GetPropertySingle (const QString &Device, const QString &Property, QVariant &Var)
{
   Var = CallMethodSingle (Device, "PropertyExists", Property);
   if (Var.toBool())
        Var = CallMethodSingle (Device, "GetProperty", Property);
   else Var = QVariant();  // Construct an empty, invalid QVariant to show that the property doesn't exist

   return NO_ERROR;
}

bool t_ThreadScanWorkerHAL::PropertyContains (const QString &Device, const QString &Property, const QString &Str)
{
   QList<QVariant> PropertyElements;
   bool            Found = false;

   CHK_EXIT (GetProperty (Device, Property, PropertyElements))

   foreach (QVariant Element, PropertyElements)
   {
      if (Element.isValid())
      {
         switch (Element.type())
         {
            case QVariant::String    : Found = (Element.toString() == Str);                                  break;
            case QVariant::StringList: Found =  Element.toStringList().contains (Str, Qt::CaseInsensitive);  break;
            default                  : break;
         }
      }
      if (Found)
         break;
   }
   return Found;
}

void t_ThreadScanWorkerHAL::SlotRescan (void)
{
   t_pDeviceList   pDeviceList=nullptr;
   t_pDevice       pDevice;
   QDBusMessage     Message;
   QDBusMessage     Reply;
   QStringList      DeviceList;
   QVariant         Category;
   QVariant         SerialNumber;
   QVariant         LinuxDevice;
   QVariant         Vendor;
   QVariant         Product;
   QVariant         Size;
   QVariant         Removable;
   QVariant         RemovableAvailable;
   QString          Model;

   CHK_EXIT (poThreadScan->SetDebugMessage ("SlotRescan called (HAL)"))

   if (thread() != QThread::currentThread())
      CHK_EXIT (ERROR_THREADSCAN_CALLED_FROM_WRONG_THREAD) // As Qt's DBus system is quite sensible to this kind of
                                                           // mistake (resulting in many QTimer "cannot be stopped/started
                                                           // from another thread) we prefer to do the check here!
   emit (SignalScanStarted ());
   LOG_INFO ("Device scan started")

   Message = QDBusMessage::createMethodCall (HAL_SERVICE, HAL_MANAGER_PATH, HAL_MANAGER_IF, "GetAllDevices");
   Reply   = pOwn->pDBusConnection->call (Message);
   if (Reply.type() == QDBusMessage::ErrorMessage)
   {
      LOG_ERROR ("DBus GetAllDevices returned %s", QSTR_TO_PSZ(Reply.errorName()))
   }
   else
   {
      pDeviceList = new t_DeviceList;
      DeviceList  = Reply.arguments().first().toStringList();

      //lint -save -e155 -e521
      foreach (QString HalDevice, DeviceList)
      //lint -restore
      {
         // Do not check the info.category any longer. It has been observed, that it may contain many different
         // strings. For example, a phone had "portable_audio_player" in its info.category and was not detected,
         // eventhough it was perfectly readable by dd.
//         CHK_EXIT (GetPropertySingle (HalDevice, "info.category", Category))
//         if (!Category.isValid())
//            continue;
//         if (Category != "storage")
//            continue;

         CHK_EXIT (GetPropertySingle (HalDevice, "block.device", LinuxDevice))
         if (!LinuxDevice.isValid())
            continue;

         if (PropertyContains (HalDevice, "info.capabilities", "volume"))  // we only are interested in complete device (/dev/sda), not in volumes (/dev/sda1)
            continue;

         CHK_EXIT (GetPropertySingle (HalDevice, "storage.removable", Removable))
         if (!Removable.isValid())
         {
            LOG_INFO ("Strange, %s is a block device but has no storage.removable property", QSTR_TO_PSZ(HalDevice))
            continue;
         }
         if (Removable.toBool())
         {
            CHK_EXIT (GetPropertySingle (HalDevice, "storage.removable.media_available", RemovableAvailable))
            if (RemovableAvailable.toBool())
                 CHK_EXIT (GetPropertySingle (HalDevice, "storage.removable.media_size", Size))
            else continue;
         }
         else
         {
            CHK_EXIT (GetPropertySingle (HalDevice, "storage.size", Size))
         }

         CHK_EXIT (GetPropertySingle (HalDevice, "storage.serial", SerialNumber))
         CHK_EXIT (GetPropertySingle (HalDevice, "info.vendor"   , Vendor      ))
         CHK_EXIT (GetPropertySingle (HalDevice, "info.product"  , Product     ))

         #ifdef DEBUG_SERNR
            #define VARIANT_TO_STRING_INFO(Var) \
               Var.isValid() ?  Var.canConvert(QVariant::String) ? QSTR_TO_PSZ(Var.toString()) : "--CANNOT CONVERT--" : "--INVALID DATA--"

            printf ("\n----- DBus/HAL device info ---------");
            printf ("\nblock.device:                 [%s]", VARIANT_TO_STRING_INFO (LinuxDevice ));
            printf ("\nstorage.serial:               [%s]", VARIANT_TO_STRING_INFO (SerialNumber));
            printf ("\ninfo.vendor:                  [%s]", VARIANT_TO_STRING_INFO (Vendor      ));
            printf ("\ninfo.product:                 [%s]", VARIANT_TO_STRING_INFO (Product     ));
            printf ("\nstorage.removable.media_size: %llu", Size.toULongLong());
            printf ("\n");
         #endif

         if (!SerialNumber.isValid())
            SerialNumber = "";  // Attention: Empty string must be used, else t_DeviceList::MatchDevice doesn't work
         if (!Vendor.isValid() && !Product.isValid())
              Model = "--";
         else Model = Vendor.toString() + " " + Product.toString();
         Model = Model.trimmed();

         pDevice = new t_Device (SerialNumber.toString(), LinuxDevice.toString(), Model,
                                 DEVICE_DEFAULT_SECTOR_SIZE, DEVICE_DEFAULT_SECTOR_SIZE, Size.toULongLong());
         if (ThreadScanIsHidden(pDevice))
         {
            delete pDevice;
         }
         else
         {
            pDeviceList->append (pDevice);
            pDevice->Removable = Removable.toBool();

            if (CONFIG(QueryDeviceMediaInfo))
               CHK_EXIT (pDevice->MediaInfo.QueryDevice(QSTR_TO_PSZ(LinuxDevice.toString())))
            CHK_EXIT (ThreadScanSetLocal        (pDevice));
            CHK_EXIT (ThreadScanGetAddStateInfo (pDevice));
         }
      }
   }
   LOG_INFO ("Device scan finished")

   CHK_EXIT (poThreadScan->SetDebugMessage ("Leaving SlotRescan (HAL)"))
   emit (SignalScanFinished (pDeviceList));
}

// --------------------------
//  t_ThreadScanWorkerDevKit
// --------------------------

// Scan the devices using DBus/UDisks, or, if not available, using DBUS/DeviceKit-Disks

typedef struct
{
   const char *pService;
   const char *pDisksIf;
   const char *pDisksPath;
   const char *pDeviceIf;
   const char *pPropertiesIf;
} t_Paths, *t_pPaths;

const t_Paths DevKitPaths = { "org.freedesktop.DeviceKit.Disks",
                              "org.freedesktop.DeviceKit.Disks",
                             "/org/freedesktop/DeviceKit/Disks",
                              "org.freedesktop.DeviceKit.Disks.Device",
                              "org.freedesktop.DBus.Properties"};

const t_Paths UDisksPaths = { "org.freedesktop.UDisks",
                              "org.freedesktop.UDisks",
                             "/org/freedesktop/UDisks",
                              "org.freedesktop.UDisks.Device",
                              "org.freedesktop.DBus.Properties"};


class t_ThreadScanWorkerDevKitLocal
{
   public:
      t_ThreadScanWorkerDevKitLocal (void)
      {
         pDBusConnection = new QDBusConnection (QDBusConnection::systemBus());
         pDBusInterface  = nullptr;
      }

     ~t_ThreadScanWorkerDevKitLocal ()
      {
         QDBusConnection::disconnectFromBus (pDBusConnection->baseService());
         delete pDBusConnection;
         pDBusConnection = nullptr;
         pDBusInterface  = nullptr;
      }

   public:
      const t_Paths            *pPaths;
      QDBusConnection          *pDBusConnection;
      QDBusConnectionInterface *pDBusInterface;
};


t_ThreadScanWorkerDevKit::t_ThreadScanWorkerDevKit (t_ThreadScan *pThreadScan, APIRET &rc)
   :t_ThreadScanWorker (pThreadScan)
{
   QDBusMessage Message;

   // Check DBUS
   // ----------
   pOwn = new t_ThreadScanWorkerDevKitLocal;
   if (!pOwn->pDBusConnection->isConnected())
   {
      LOG_INFO ("DBus connection failed")
      rc = ERROR_THREADSCAN_DBUSDEVKIT_NOTWORKING;
      return;
   }

   // Do a UDisks method call first
   // -----------------------------
   // The reason is, that Kubuntu only starts its udisks daemon when needed: "udisks-daemon provides the
   // org.freedesktop.UDisks service on the system message bus. Users or administrators should never need to start
   // this daemon as it will automatically started by dbus-daemon(1) whenever an application calls into the
   // org.freedesktop.UDisks service." They only forgot that a good program first asks if it's there before accessing
   // it... BTW, the ipod program "clementine" faced the same problem.

   pOwn->pPaths = &UDisksPaths;
   Message = QDBusMessage::createMethodCall (pOwn->pPaths->pService, pOwn->pPaths->pDisksPath, pOwn->pPaths->pDisksIf, "EnumerateDevices");
   QDBusReply<QDBusArgument> Reply = pOwn->pDBusConnection->call(Message);
   if (!Reply.isValid())
     LOG_INFO ("Test method call for starting udisks daemon didn't seem to work")


   // Check if either UDisks or DeviceKit is alive
   // --------------------------------------------
   pOwn->pDBusInterface = pOwn->pDBusConnection->interface();
   pOwn->pPaths = &UDisksPaths;
   if (!pOwn->pDBusInterface->isServiceRegistered (pOwn->pPaths->pService))
   {
      LOG_INFO ("%s not registered", pOwn->pPaths->pService)
      pOwn->pPaths = &DevKitPaths;
      if (!pOwn->pDBusInterface->isServiceRegistered (pOwn->pPaths->pService))
      {
         LOG_INFO ("%s not registered", pOwn->pPaths->pService)
         rc = ERROR_THREADSCAN_DBUSDEVKIT_NOTWORKING;
         return;
      }
   }

   LOG_INFO ("Using %s", pOwn->pPaths->pService)
   rc = NO_ERROR;
}

t_ThreadScanWorkerDevKit::~t_ThreadScanWorkerDevKit (void)
{
   delete pOwn;
}

QList<QVariant> t_ThreadScanWorkerDevKit::CallMethod (const QString &Device, const QString &Method, const QString &Argument)
{
   QList<QVariant> Args;
   QDBusMessage    Message = QDBusMessage::createMethodCall (pOwn->pPaths->pService, Device, pOwn->pPaths->pDeviceIf, Method);
   QDBusMessage    Reply;

   Args += Argument;
   Message.setArguments (Args);
   Reply = pOwn->pDBusConnection->call (Message);

   if (Reply.type() == QDBusMessage::ErrorMessage)
   {
      LOG_ERROR ("DBus returned '%s' for method %s on device %s", QSTR_TO_PSZ (Reply.errorName()),
                                                                  QSTR_TO_PSZ (Method),
                                                                  QSTR_TO_PSZ (Device))
      return QList<QVariant>(); // return empty list
   }

   return Reply.arguments();
}

APIRET t_ThreadScanWorkerDevKit::GetProperty (const QString &Device, const QString &Property, QVariant &Var)
{
   QDBusMessage             Message = QDBusMessage::createMethodCall (pOwn->pPaths->pService, Device, pOwn->pPaths->pPropertiesIf, "Get");  // Query DBUS object properties
   QList     <QVariant>     Args;
   QDBusReply<QDBusVariant> Reply;

   Args << Device << Property;
   Message.setArguments (Args);
   Reply = pOwn->pDBusConnection->call (Message);

   if (!Reply.isValid())
   {
      // TODO: Extract error from Reply.error()
      //LOG_ERROR ("DBus returned '%s' while retrieving property %s for device %s", QSTR_TO_PSZ (Reply.error()),
      //                                                                            QSTR_TO_PSZ (Property),
      //                                                                            QSTR_TO_PSZ (Device))
      Var = QVariant(); // return empty QVariant
   }
   else
   {
      QVariant ReplyArg = Reply.value().variant();  // The method returns a QDBusVariant, that must be
      if (ReplyArg.isNull())                        // converted to a QVariant before it can be used!
           Var = QVariant();
      else Var = ReplyArg;
   }

   return NO_ERROR;
}

void t_ThreadScanWorkerDevKit::SlotRescan (void)
{
   t_pDeviceList   pDeviceList=nullptr;
   t_pDevice       pDevice;
   QDBusMessage    Message;
   QStringList     DeviceList;
   QVariant        Category;
   QVariant        SerialNumber;
   QVariant        LinuxDevice;
   QVariant        NativePath;
   QVariant        ByPath;
   QVariant        Interface;
   QVariant        Vendor;
   QVariant        Product;
   QVariant        Size;
   QVariant        BlockSize;
   QVariant        Drive;
   QVariant        Removable;
   QVariant        RemovableAvailable;
   QString         Model;

   CHK_EXIT (poThreadScan->SetDebugMessage ("SlotRescan called (DevKit)"))

   if (thread() != QThread::currentThread())
      CHK_EXIT (ERROR_THREADSCAN_CALLED_FROM_WRONG_THREAD) // As Qt's DBus system is quite sensible to this kind of
                                                           // mistake (resulting in many QTimer "cannot be stopped/started
                                                           // from another thread) we prefer to do the check here!
   emit (SignalScanStarted ());
   LOG_INFO ("Device scan started")

   Message = QDBusMessage::createMethodCall (pOwn->pPaths->pService, pOwn->pPaths->pDisksPath, pOwn->pPaths->pDisksIf, "EnumerateDevices");
   QDBusReply<QDBusArgument> Reply = pOwn->pDBusConnection->call(Message);
   if (!Reply.isValid())
   {
      LOG_ERROR ("DBus DevKit::EnumerateDevices failed")
   }
   else
   {
      pDeviceList = new t_DeviceList;

      // EnumerateDevices returns a QDBusArgument argument containing a list of QDBusObjectPath objects.
      // Using qdbus_cast was the only method I found that converted the whole crap to a normal QStringList.
      QDBusArgument  ReplyArgument = Reply.value();
      QList<QString> DevKitDevices = qdbus_cast<QList<QString> >(ReplyArgument);

      //lint -save -e155 -e521
      foreach (QString DevKitDevice, DevKitDevices)
      //lint -restore
      {
         CHK_EXIT (GetProperty (DevKitDevice, "device-file", LinuxDevice))
         if (!LinuxDevice.isValid ())     continue;
         if ( LinuxDevice.toString()=="") continue;

         CHK_EXIT (GetProperty (DevKitDevice, "device-is-drive", Drive))
         if(!Drive.isValid()) continue;
         if(!Drive.toBool ()) continue;

         CHK_EXIT (GetProperty (DevKitDevice, "device-is-removable", Removable))
         if (!Removable.isValid())
         {
            LOG_INFO ("Strange, %s is a block device but has no device-is-removable property", QSTR_TO_PSZ(DevKitDevice))
            continue;
         }

         if (Removable.toBool())
         {
            CHK_EXIT (GetProperty (DevKitDevice, "device-is-media-available", RemovableAvailable))
            if (!RemovableAvailable.toBool())
               continue;
         }

         CHK_EXIT (GetProperty (DevKitDevice, "device-size"               , Size        ))
         CHK_EXIT (GetProperty (DevKitDevice, "drive-serial"              , SerialNumber))
         CHK_EXIT (GetProperty (DevKitDevice, "drive-vendor"              , Vendor      ))
         CHK_EXIT (GetProperty (DevKitDevice, "drive-model"               , Product     ))
         CHK_EXIT (GetProperty (DevKitDevice, "device-block-size"         , BlockSize   ))
         CHK_EXIT (GetProperty (DevKitDevice, "native-path"               , NativePath  ))
         CHK_EXIT (GetProperty (DevKitDevice, "device-file-by-path"       , ByPath      ))
         CHK_EXIT (GetProperty (DevKitDevice, "drive-connection-interface", Interface   ))

         CHK_EXIT (ThreadScanCheckForceSerialNumber (QSTR_TO_PSZ(LinuxDevice.toString()), &SerialNumber))

         #ifdef DEBUG_SERNR
              printf ("\n----- DBus/DevKit-UDisks device info ---------");
              printf ("\ndevice-file:                 [%s]", VARIANT_TO_STRING_INFO (LinuxDevice ));
              printf ("\ndrive-serial:                [%s]", VARIANT_TO_STRING_INFO (SerialNumber));
              printf ("\ndrive-vendor:                [%s]", VARIANT_TO_STRING_INFO (Vendor      ));
              printf ("\ndrive-model:                 [%s]", VARIANT_TO_STRING_INFO (Product     ));
              printf ("\ndevice-size:                 %llu", Size     .toULongLong());
              printf ("\ndevice-block-size:           %llu", BlockSize.toULongLong());
              printf ("\n");
         #endif

         if (!SerialNumber.isValid())
            SerialNumber = "";  // Attention: Empty string must be used, else t_DeviceList::MatchDevice doesn't work
         if (!Vendor.isValid() && !Product.isValid())
              Model = "--";
         else Model = Vendor.toString() + " " + Product.toString();
         Model = Model.trimmed();


         pDevice = new t_Device (SerialNumber.toString(), LinuxDevice.toString(), Model,
                                 BlockSize.toULongLong(), BlockSize.toULongLong(), Size.toULongLong(),
                                 NativePath.toString(), ByPath.toString(), Interface.toString());
         if (ThreadScanIsHidden(pDevice))
         {
            delete pDevice;
         }
         else
         {
            pDeviceList->append (pDevice);
            pDevice->Removable = Removable.toBool();
            if (CONFIG(QueryDeviceMediaInfo))
               CHK_EXIT (pDevice->MediaInfo.QueryDevice(QSTR_TO_PSZ(LinuxDevice.toString())))
            CHK_EXIT (ThreadScanSetLocal        (pDevice));
            CHK_EXIT (ThreadScanGetAddStateInfo (pDevice));
         }
      }
   }
   LOG_INFO ("Device scan finished")

   CHK_EXIT (poThreadScan->SetDebugMessage ("Leaving SlotRescan (DevKit)"))
   emit (SignalScanFinished (pDeviceList));
}

// --------------------------
//        t_ThreadScan
// --------------------------

static APIRET ThreadScanRegisterErrorCodes (void)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      Initialised = true;
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_NOT_STARTED             ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_NOT_STOPPED             ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_EXITCODE_NONZERO        ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_PROCESS_NOTSTARTED      ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_PROCESS_NOTFINISHED     ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_LIBPARTED_NOTWORKING    ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_LIBUDEV_NOTWORKING      ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_DBUSHAL_NOTWORKING      ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_DBUSDEVKIT_NOTWORKING   ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_PROPERTY_NONEXISTENT    ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_CALLED_FROM_WRONG_THREAD))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_INVALID_SCAN_METHOD     ))
      CHK (TOOL_ERROR_REGISTER_CODE (ERROR_THREADSCAN_UDEV_FAILURE            ))
   }
   return NO_ERROR;
}

t_ThreadScan::t_ThreadScan (void)
{
   CHK_EXIT (ThreadScanRegisterErrorCodes())
   ppoWorker = nullptr;  //lint -esym(613, t_ThreadScan::ppoWorker)  Possible use of nullptr
}


static const char *ThreadScanMethodToString (t_CfgScanMethod Method)
{
   const char *pStr=nullptr;

   switch (Method)
   {
      case SCANMETHOD_DBUSDEVKIT: pStr = "DBus/Udisks"; break;
      case SCANMETHOD_DBUSHAL:    pStr = "DBus/HAL"   ; break;
      case SCANMETHOD_LIBPARTED:  pStr = "libparted"  ; break;
      case SCANMETHOD_LIBUDEV:    pStr = "libudev"    ; break;
      default: CHK_EXIT (ERROR_THREADSCAN_INVALID_SCAN_METHOD)
   }
   return pStr;
}


static APIRET ThreadScanAskUser (bool &Ok)
{
   static QList<t_CfgScanMethod>   MethodsAlreadyTried;
   const char                    *pMethodStr;
   QPushButton                   *pButton;
   QPushButton                   *pButtonAbort;
   QButtonGroup                    ButtonGroup;
   QMessageBox                     MessageBox;
   t_CfgScanMethod                 Method;

   MethodsAlreadyTried.append (CONFIG (DeviceScanMethod));
   pMethodStr = ThreadScanMethodToString (CONFIG (DeviceScanMethod));
   LOG_INFO ("Scan method %s not accessible, asking user if he wants to switch to another one", pMethodStr)

   MessageBox.setText           (QObject::tr("Guymager cannot scan the devices connected to this computer."));
   MessageBox.setInformativeText(QObject::tr("The selected scan method (\"%1\") is not available. Do you want to try another scan method?") .arg (pMethodStr));
   pButtonAbort = MessageBox.addButton (QObject::tr("Abort"), QMessageBox::AcceptRole);

   for (Method = (t_CfgScanMethod)0;
        Method < SCANMETHOD_COUNT;
        Method = (t_CfgScanMethod)(Method+1))
   {
      if (!MethodsAlreadyTried.contains (Method))
      {
         pMethodStr = ThreadScanMethodToString (Method);
         pButton = MessageBox.addButton (QObject::tr("Try method \"%1\"") .arg(pMethodStr), QMessageBox::AcceptRole);
         ButtonGroup.addButton (pButton, Method);
      }
   }

   MessageBox.exec();
   Ok = (MessageBox.clickedButton() != pButtonAbort);
   if (Ok)
   {
      Method = (t_CfgScanMethod) ButtonGroup.id (MessageBox.clickedButton());
      LOG_INFO ("User switches to %s", ThreadScanMethodToString (Method))
      CONFIG (DeviceScanMethod) = Method;
   }
   else
   {
      LOG_INFO ("User wants to abort. Exiting now.")
   }
   return NO_ERROR;
}


APIRET t_ThreadScan::Start (t_ThreadScanWorker **ppWorker)
{
   int  Wait;
   int  Tries;
   bool Ok;

   CHK_EXIT (SetDebugMessage ("Launch thread"))

   #define LAUNCH_WORKER                                                                      \
   {                                                                                          \
      *ppWorker   = nullptr;                                                                  \
      ppoWorker   = ppWorker;                                                                 \
        oWorkerRc = NO_ERROR;                                                                 \
      start();                                                                                \
      /* start(QThread::HighPriority); */                                                     \
      for ( Wait =  0;                                                                        \
           (Wait <  THREADSCAN_WAIT_MAX) && (*ppWorker == nullptr) && (oWorkerRc==NO_ERROR);  \
            Wait += THREADSCAN_WAIT_GRANULARITY)                                              \
         msleep (THREADSCAN_WAIT_GRANULARITY);                                                \
   }

   for (Tries=1;;Tries++)  // Try all scan methods if necessary
   {
      LAUNCH_WORKER;
      if ((oWorkerRc == ERROR_THREADSCAN_LIBPARTED_NOTWORKING ) ||
          (oWorkerRc == ERROR_THREADSCAN_LIBUDEV_NOTWORKING   ) ||
          (oWorkerRc == ERROR_THREADSCAN_DBUSHAL_NOTWORKING   ) ||
          (oWorkerRc == ERROR_THREADSCAN_DBUSDEVKIT_NOTWORKING) ||
          (Wait      >=  THREADSCAN_WAIT_MAX))
      {
         if (Tries == SCANMETHOD_COUNT)
         {
            t_MessageBox::critical (nullptr, tr ("Cannot scan devices", "Dialog title"),
                                             tr ("None of the device scan methods worked. Exiting now."));
            break;
         }
         CHK (ThreadScanAskUser (Ok))
         if (!Ok)
            break;
      }
      else
      {
         break;
      }
   }
   #undef LAUNCH_WORKER

   CHK (oWorkerRc)
   if (*ppWorker == nullptr)
      CHK (ERROR_THREADSCAN_NOT_STARTED)

   CHK_EXIT (SetDebugMessage ("Thread launched"))

   return NO_ERROR;
}

APIRET t_ThreadScan::Stop ()
{
   time_t BeginT, NowT;
   int    Wait;

   quit(); // This tells t_ThreadScan::run to quit the exec() call

   time (&BeginT);  // As we do not know how much time is spent in the different calls
   do               // to processEvents, we have to measure the time ourselves
   {
      QCoreApplication::processEvents (QEventLoop::ExcludeUserInputEvents, THREADSCAN_WAIT_GRANULARITY);
      time (&NowT);
      Wait = (NowT-BeginT)*MSEC_PER_SECOND;
   } while ((Wait < THREADSCAN_WAIT_MAX) && (*ppoWorker != nullptr));

   if (*ppoWorker != nullptr)
      CHK (ERROR_THREADSCAN_NOT_STOPPED)

   return NO_ERROR;
}


void t_ThreadScan::run (void)
{
   t_ThreadScanWorker *pWorker   = nullptr;
   APIRET               WorkerRc = NO_ERROR;
   QTimer             *pTimer;
   int                    rc;

   LOG_INFO ("Thread Scan started")
   CHK_EXIT (SetDebugMessage ("Start run function"))

   switch (CONFIG(DeviceScanMethod))
   {
      // We have to create the following object as we want to work with signals and slots in this new thread.
      // t_ThreadScan itself belongs to the main thread and thus can't be used for signals and slots.
      case SCANMETHOD_DBUSDEVKIT: pWorker = new t_ThreadScanWorkerDevKit (this, WorkerRc); break;
      case SCANMETHOD_DBUSHAL:    pWorker = new t_ThreadScanWorkerHAL    (this, WorkerRc); break;
      case SCANMETHOD_LIBPARTED:  pWorker = new t_ThreadScanWorkerParted (this, WorkerRc); break;
      case SCANMETHOD_LIBUDEV:    pWorker = new t_ThreadScanWorkerUdev   (this, WorkerRc); break;
      default: CHK_EXIT (ERROR_THREADSCAN_INVALID_SCAN_METHOD)
   }
   *ppoWorker = pWorker;
   oWorkerRc  = WorkerRc;

   if (oWorkerRc)
      return;

//   pTimer = new QTimer(this); // Do not take "this" as parent, as "this" doesn't belong to the current thread (would lead to the error "Cannot create children for a parent that is in a different thread").
   pTimer = new QTimer();
   CHK_QT_EXIT (connect (pTimer, SIGNAL(timeout()), *ppoWorker, SLOT(SlotRescan())))
   pTimer->start (CONFIG(ScanInterval) * MSEC_PER_SECOND);

   rc = exec(); // Enter event loop
   if (rc)
   {
      LOG_ERROR ("ThreadScan exits with code %d", rc)
      CHK_EXIT (ERROR_THREADSCAN_EXITCODE_NONZERO)
   }
   LOG_INFO ("Thread Scan ended")
   CHK_EXIT (SetDebugMessage ("Exit run function"))
   delete pTimer;
   delete *ppoWorker;
   *ppoWorker = nullptr;
}



