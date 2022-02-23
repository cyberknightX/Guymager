// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Provides all information for displaying the DeviceList on
//                  screen
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

#ifndef DEVICELISTMODEL_H
#define DEVICELISTMODEL_H

#include <QObject>
#include <QList>
#include <QAbstractTableModel>

#include "device.h"


class t_DeviceListModel: public QAbstractTableModel
{
   Q_OBJECT

   public:
      t_DeviceListModel ();
      t_DeviceListModel (t_pDeviceList pDeviceList);

      int  rowCount    (const QModelIndex & parent= QModelIndex()) const;
      int  columnCount (const QModelIndex & parent= QModelIndex()) const;

      QVariant data       (const QModelIndex &Index, int Role) const;
      QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

   public slots:
      void SlotRefresh (void);
//      void SlotUpdate  (void);


   public:
      typedef QVariant (* t_pGetDataFn) (t_pDevice pDevice);

   private:
      enum
      {
         DATATYPE_QSTRING,
         DATATYPE_INT32,
         DATATYPE_INT64,
         DATATYPE_DEVICE_STATE,
         DATATYPE_ACQUIRE_STATE,
      };

      typedef struct
      {
         QString       Name;         // The column name as displayed in the table header
         t_pGetDataFn pGetDataFn;
         int           DisplayType;  // One of the display type as defined in t_ItemDelegate
         int           Alignment;    // Use Qt::AlignLeft, Qt::AlignRight, ...
         int           MinWidth;     // Set to 0 for default width
      } t_ColAssoc;

      QList<t_ColAssoc> ColAssocList;

      t_pDeviceList poDeviceList;
};


#endif
