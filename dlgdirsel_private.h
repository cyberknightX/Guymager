// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Private definitions of directory selection dialog
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

#ifndef DLGDIRSEL_PRIVATE_H
#define DLGDIRSEL_PRIVATE_H

#ifndef COMMON_H
   #include "common.h"
#endif

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif

class t_TreeView: public QTreeView
{
   Q_OBJECT

   public:
      t_TreeView (QWidget *pParent=nullptr)
         :QTreeView (pParent)
      {
      }

     ~t_TreeView (void)
      {
      }

   protected:
      virtual void currentChanged (const QModelIndex &Current, const QModelIndex &Previous)
      {
         QTreeView::currentChanged (Current, Previous);
         emit SignalCurrentChanged (Current);
      }

   signals:
      void SignalCurrentChanged (const QModelIndex &Current);
};

#endif

