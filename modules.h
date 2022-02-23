// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Central application error code managment
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

#ifndef MODULES_H
#define MODULES_H

#define CREATE_MODULE_IDS(ModulName, x) \
   /*lint -save -e835 -e845*/           \
   const unsigned int ERROR_BASE_ ## ModulName = ERROR_BASE_GUYMAGER_MAIN +  x * ID_OFFSET_SUBSUBPROJECT; \
   /*lint -restore*/


CREATE_MODULE_IDS (MAIN           ,  0)
CREATE_MODULE_IDS (LOG            ,  1)
CREATE_MODULE_IDS (CFG            ,  2)
CREATE_MODULE_IDS (UTIL           ,  3)
CREATE_MODULE_IDS (QTUTIL         ,  4)
CREATE_MODULE_IDS (SCAN           ,  5)
CREATE_MODULE_IDS (MAINWINDOW     ,  6)
CREATE_MODULE_IDS (TABLE          ,  7)
CREATE_MODULE_IDS (FIFO           ,  8)
CREATE_MODULE_IDS (THREADSCAN     ,  9)
CREATE_MODULE_IDS (THREADREAD     , 10)
CREATE_MODULE_IDS (THREADWRITE    , 11)
CREATE_MODULE_IDS (THREADHASH     , 12)
CREATE_MODULE_IDS (THREADCOMPRESS , 13)
CREATE_MODULE_IDS (DEVICE         , 14)
CREATE_MODULE_IDS (DEVICELISTMODEL, 15)
CREATE_MODULE_IDS (FILE           , 16)
CREATE_MODULE_IDS (ITEMDELEGATE   , 17)
CREATE_MODULE_IDS (INFO           , 18)
CREATE_MODULE_IDS (INFOFIELD      , 19)
CREATE_MODULE_IDS (DLGACQUIRE     , 20)
CREATE_MODULE_IDS (DLGAUTOEXIT    , 21)
CREATE_MODULE_IDS (DLGMESSAGE     , 22)
CREATE_MODULE_IDS (DLGABORT       , 23)
CREATE_MODULE_IDS (DLGDIRSEL      , 24)
CREATE_MODULE_IDS (DLGWAIT        , 25)
CREATE_MODULE_IDS (HASH           , 26)
CREATE_MODULE_IDS (AAFF           , 27)
CREATE_MODULE_IDS (AEWF           , 28)
CREATE_MODULE_IDS (MEDIA          , 29)
CREATE_MODULE_IDS (RUNSTATS       , 30)

CREATE_MODULE_IDS (QT             , 99)

enum
{
   ERROR_QT_UNSUCCESSFUL = (ERROR_BASE_QT + 1)
}; //lint !e30


#endif

