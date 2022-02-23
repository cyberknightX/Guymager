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

#if (QT_VERSION >= 0x050000)
   #include <QtWidgets> //lint !e537 Repeated include
#else
   #include <QtGui>     //lint !e537 Repeated include
#endif
#include <QFontMetrics>

#include "common.h"
#include "itemdelegate.h"

#include <device.h>
#include <config.h>

class t_ItemDelegateLocal
{
   public:
};

t_ItemDelegate::t_ItemDelegate (QObject *pParent)
   : QItemDelegate (pParent)
{
   static bool Initialised = false;

   if (!Initialised)
   {
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_ITEMDELEGATE_BAD_DISPLAY_TYPE))
      Initialised = true;
   }

   pOwn = new t_ItemDelegateLocal;
}

t_ItemDelegate::~t_ItemDelegate ()
{
   delete pOwn;
}

// PaintDefaults: default background & cursor painting
// ---------------------------------------------------
void t_ItemDelegate::PaintDefaults (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index, QColor &ColorPen) const
{
   QPalette::ColorGroup ColorGroup;

   ColorGroup = Option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
   if (ColorGroup == QPalette::Normal && !(Option.state & QStyle::State_Active))
       ColorGroup = QPalette::Inactive;

   if (Option.state & QStyle::State_Selected)
   {
      pPainter->fillRect (Option.rect, Option.palette.brush (ColorGroup, QPalette::Highlight));
      ColorPen = Option.palette.color (ColorGroup, QPalette::HighlightedText);
   }
   else
   {
      QVariant Variant = Index.data (Qt::BackgroundRole);
      pPainter->fillRect (Option.rect, Variant.value<QBrush>());
      ColorPen = Option.palette.color (ColorGroup, QPalette::Text);
   }
}

void t_ItemDelegate::PaintProgress (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index) const
{
   t_pDevice pDev;
   QRect        BarRect;
   QRect        TextRect;
   QColor       ColorPen;
   double       ProgressFactor;
   int          BarSep;
   
   pPainter->save();
   PaintDefaults (pPainter, Option, Index, ColorPen);

   pDev = (t_pDevice) Index.data(DeviceRole).value<void *>();
   if (!pDev->StartTimestamp.isNull())
   {
      ProgressFactor = Index.data (Qt::DisplayRole).toDouble();

      BarSep = Option.rect.height() - pPainter->fontMetrics().ascent(); // Keep enough space above bar for percentage count

      BarRect = Option.rect;
      BarRect.adjust (10, BarSep, -10, -4);  // keep progress bar rectangle at 10 px from left and right and 4 px from bottom
      pPainter->setBrush (Qt::NoBrush);
      pPainter->setPen   (ColorPen);
      pPainter->drawRect (BarRect);

      TextRect = Option.rect;
      TextRect.setHeight (BarSep + pPainter->fontMetrics().descent()/2); // As we only show now numbers and thus not use the descent space (unlike g, j etc.) we move down the text a bit to make it appear vertically centered in the foreseen text area
      pPainter->drawText (TextRect, Qt::AlignHCenter | Qt::AlignCenter, QString ("%1%") .arg ((int)(ProgressFactor*100)) );

      pPainter->setBrush (ColorPen);
      pPainter->setPen   (Qt::NoPen);
      BarRect.setWidth ((int) (BarRect.width() * ProgressFactor));
      pPainter->drawRect (BarRect);
   }
   pPainter->restore  ();
}

static void ItemDelegateStateCircleParams (const QStyleOptionViewItem &Option, int *pCenterDistance, int *pDiameter=nullptr)
{
   int Diameter;

   Diameter = Option.rect.height() / 2;
   if (pCenterDistance) *pCenterDistance = (Diameter * 3) / 4;
   if (pDiameter      ) *pDiameter = Diameter;
}

void t_ItemDelegate::PaintState (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index) const
{
   QColor               ColorPen;
   QString              StateStr;
   QStyleOptionViewItem OptionModified (Option);
   QRect              *pDrawRect;
   int                  CircleDiameter;
   int                  CircleCenterDistance;
   t_CfgColor           CircleColor;
   QRect                CircleRect;
   t_pDevice           pDev;

   pPainter->save();
   PaintDefaults (pPainter, Option, Index, ColorPen);

   pDev = (t_pDevice) Index.data(DeviceRole).value<void *>();

   switch (pDev->GetState())
   {
      case t_Device::Idle         : CircleColor = COLOR_STATE_IDLE;           break;
      case t_Device::Queued       : CircleColor = COLOR_STATE_QUEUED;         break;
      case t_Device::Acquire      : CircleColor = COLOR_STATE_ACQUIRE;        break;
      case t_Device::Launch       : CircleColor = COLOR_STATE_ACQUIRE;        break;
      case t_Device::LaunchPaused : CircleColor = COLOR_STATE_ACQUIRE_PAUSED; break;
      case t_Device::AcquirePaused: CircleColor = COLOR_STATE_ACQUIRE_PAUSED; break;
      case t_Device::Verify       : CircleColor = COLOR_STATE_VERIFY;         break;
      case t_Device::VerifyPaused : CircleColor = COLOR_STATE_VERIFY_PAUSED;  break;
      case t_Device::Cleanup      : CircleColor = COLOR_STATE_CLEANUP;        break;
      case t_Device::Finished     : if (pDev->HashMatch())
                                    {
                                       if (pDev->Duplicate && (pDev->Error.Get(1) || pDev->Error.Get(2)))
                                            CircleColor = COLOR_STATE_FINISHED_DUPLICATE_FAILED;
                                       else CircleColor = COLOR_STATE_FINISHED;
                                    }
                                    else
                                    {
                                       CircleColor = COLOR_STATE_FINISHED_BADVERIFY;
                                    }
                                    break;
      case t_Device::Aborted      : if (pDev->Error.Get()==t_Device::t_Error::UserRequest)
                                         CircleColor = COLOR_STATE_ABORTED_USER;
                                    else CircleColor = COLOR_STATE_ABORTED_OTHER;
                                    break;
      default:                      CircleColor = COLOR_STATE_IDLE;
   }

   // Draw circle and text
   // --------------------
   if (!pDev->Local)
   {
      pPainter->setBrush (QBrush (CONFIG_COLOR(CircleColor)));

      ItemDelegateStateCircleParams (Option, &CircleCenterDistance, &CircleDiameter);

      pDrawRect = &OptionModified.rect;
      CircleRect.setSize    (QSize (CircleDiameter, CircleDiameter));
      CircleRect.moveCenter (QPoint(pDrawRect->left() + CircleCenterDistance,
                                    pDrawRect->top () - 1 + pDrawRect->height()/2));
      pPainter->setPen      (ColorPen);
      pPainter->drawEllipse (CircleRect);

      pDrawRect->setLeft (OptionModified.rect.left() + 2*CircleCenterDistance); // Shift text origin to the right of the circle
   }

   QItemDelegate::paint (pPainter, OptionModified, Index);

   pPainter->restore  ();
}

void t_ItemDelegate::paint (QPainter *pPainter, const QStyleOptionViewItem &Option, const QModelIndex &Index) const
{
   int DisplayType;

   DisplayType = Index.data(t_ItemDelegate::DisplayTypeRole).toInt();
   switch (DisplayType)
   {
      case DISPLAYTYPE_STANDARD: QItemDelegate::paint (pPainter, Option, Index); break;
      case DISPLAYTYPE_PROGRESS: PaintProgress        (pPainter, Option, Index); break;
      case DISPLAYTYPE_STATE:    PaintState           (pPainter, Option, Index); break;

      default:
         CHK_EXIT (ERROR_ITEMDELEGATE_BAD_DISPLAY_TYPE)
   }

}

QSize t_ItemDelegate::sizeHint (const QStyleOptionViewItem &Option, const QModelIndex &Index) const
{
   int   DisplayType;
   int   MinColWidth;
   int   StateCircleCenterDistance;
   QSize SizeHint;

   DisplayType = Index.data(t_ItemDelegate::DisplayTypeRole).toInt();
   switch (DisplayType)
   {
      case DISPLAYTYPE_STANDARD: MinColWidth = Index.data (t_ItemDelegate::MinColWidthRole).toInt();
                                 return QItemDelegate::sizeHint (Option, Index).expandedTo (QSize(MinColWidth, 0));

      case DISPLAYTYPE_PROGRESS: return QSize (10, 10); // Dummy size; Anyway, as the column title is bigger that width will be used.

      case DISPLAYTYPE_STATE   : MinColWidth  = Index.data (t_ItemDelegate::MinColWidthRole).toInt();
                                 ItemDelegateStateCircleParams (Option, &StateCircleCenterDistance);
                                 SizeHint = QItemDelegate::sizeHint (Option, Index);
                                 SizeHint.rwidth() += 2 * StateCircleCenterDistance;
                                 return SizeHint.expandedTo (QSize(MinColWidth, 0));

      default:                   CHK_EXIT (ERROR_ITEMDELEGATE_BAD_DISPLAY_TYPE)
   }

   return QSize();
}

