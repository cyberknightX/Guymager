// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         t_ItemDelegate is responsible for displaying the cells
//                  in t_Table.
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

#ifndef ITEMDELEGATE_H
#define ITEMDELEGATE_H

#ifndef COMMON_H
   #include "common.h"
#endif

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif


class t_ItemDelegateLocal;

class t_ItemDelegate: public QItemDelegate
{
   Q_OBJECT

   public:
      static const int DisplayTypeRole = Qt::UserRole;   // Extension to Qt::ItemDataRole; when QModelIndex::data is called with this role, it is expected to return one of the enums below
      static const int RowNrRole       = Qt::UserRole+1; // To find out the screen row nr / list row nr lookup
      static const int MinColWidthRole = Qt::UserRole+2; // To get the minimum desired column width as defined in t_ColAssoc;
      static const int DeviceRole      = Qt::UserRole+3; // To get pDevice

      enum
      {
         DISPLAYTYPE_STANDARD = 0,  // Use the standard QItemDelegate functions for displaying this cell
         DISPLAYTYPE_PROGRESS,      // The value returned is between 0 and 100 and should be displayed as a percentage bar
         DISPLAYTYPE_STATE          // Special drawing for the status bar
      };

   protected:
      void PaintDefaults (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index, QColor &ColorPen) const;
      void PaintProgress (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index) const;
      void PaintState    (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index) const;

   public:
      t_ItemDelegate (QObject *pParent=nullptr);
     ~t_ItemDelegate ();

      virtual void  paint    (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index) const;
      virtual QSize sizeHint (                    const QStyleOptionViewItem &Option, const QModelIndex &Index) const;

   private:
      t_ItemDelegateLocal *pOwn;
};

// ------------------------------------
//             Error codes
// ------------------------------------

#ifdef MODULES_H
   enum
   {
      ERROR_ITEMDELEGATE_BAD_DISPLAY_TYPE = ERROR_BASE_ITEMDELEGATE + 1,
   };
#endif

#endif

