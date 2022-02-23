// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Different message boxes we need all the time
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

#include "dlgmessage.h"
#include "config.h"
#include "qtutil.h"

// -----------------------------
//           Constants
// -----------------------------

const int DLGMESSAGE_MAX_SCREEN_PERCENTAGE = 80;  // When opening the dialog, do not use more than 80% of the screen width / height

// -----------------------------
//           Classes
// -----------------------------

class t_DlgMessageLocal
{
   public:
      QVBoxLayout *pLayout;
      QTextEdit   *pTextBox;
      QPushButton *pButtonClose;
};

t_DlgMessage::t_DlgMessage ()
{
   CHK_EXIT (ERROR_DLGMESSAGE_CONSTRUCTOR_NOT_SUPPORTED)
} //lint !e1401 pOwn not initialised

t_DlgMessage::t_DlgMessage (const QString &Title, QString Message, bool Monospaced, QWidget *pParent, Qt::WindowFlags Flags)
   :QDialog (pParent, Flags)
{
   static bool  Initialised = false;
   QFont      *pFont;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_DLGMESSAGE_CONSTRUCTOR_NOT_SUPPORTED))
   }

   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)

   pOwn = new t_DlgMessageLocal;
   pOwn->pLayout  = new QVBoxLayout ();
   pOwn->pTextBox = new QTextEdit   (this);

   pFont = CfgGetpFont (FONTOBJECT_DIALOG_DATA);  // Set the font, if available
   if (pFont)
      (pOwn->pTextBox)->setFont(*pFont);

   if (Monospaced && !pFont)
   {
      Message.prepend ("<pre>" ); // If no font was specified, ensure that
      Message.append  ("</pre>"); // a standard monospaced font is used
   }
   else
   {
      Message.replace ("\r\n", "<br>");       // Translate line breaks into HTML tags
      Message.replace ("\n"  , "<br>");
   }

   pOwn->pTextBox->setWordWrapMode (QTextOption::NoWrap);
   pOwn->pTextBox->setAcceptRichText (true);
   pOwn->pTextBox->setHtml (Message);

   pOwn->pButtonClose = new QPushButton (tr("Close"), this);

   pOwn->pLayout->addWidget (pOwn->pTextBox    );
   pOwn->pLayout->addWidget (pOwn->pButtonClose);

   pOwn->pTextBox->setReadOnly (true);
   pOwn->pTextBox->zoomIn ();

   setLayout (pOwn->pLayout);
   setWindowTitle (Title);

   CHK_QT_EXIT (connect (pOwn->pButtonClose, SIGNAL (released()), this, SLOT(accept())))
}

void t_DlgMessage::AdjustGeometry (void)
{
   int X, Y;
   int Dx, Dy;

   // Adjust size
   // -----------
   // Trying to adjust the size of the dialog to size of the text inside. Not easy, as Qt doesn't provide the right functions for
   // doing so. Important: AdjustSize must be called after the dialog has been showed in order have QTextBox calculated all scroll
   // bar values.
   Dx  = pOwn->pTextBox->horizontalScrollBar()->maximum();     // See source code of QAbstractScrollArea: The scroll bar range is the full
   Dy  = pOwn->pTextBox->verticalScrollBar  ()->maximum();     // scroll area width minus the window area displayed. So, we, take the
   Dx += pOwn->pTextBox->viewport()->width ();                 // scroll bar range and add the viewport size.
   Dy += pOwn->pTextBox->viewport()->height();
   Dx += pOwn->pTextBox->verticalScrollBar  ()->height();      // Scrollbars need some place as well...
   Dy += pOwn->pTextBox->horizontalScrollBar()->height();
   Dx += frameGeometry().width () - pOwn->pTextBox->width ();  // Add the space surrounding the Textbox required by the dialog
   Dy += frameGeometry().height() - pOwn->pTextBox->height();

   Dx = GETMIN (Dx, (DLGMESSAGE_MAX_SCREEN_PERCENTAGE * QApplication::desktop()->width ()) / 100);  // Limit to a certain amount
   Dy = GETMIN (Dy, (DLGMESSAGE_MAX_SCREEN_PERCENTAGE * QApplication::desktop()->height()) / 100);  // of the available screen space


   // Adjust position
   // ---------------
   // Try to center relative to the parent if available, relative to srceen otherwise

   QWidget *pParent = parentWidget();
   if (pParent)
   {
      QPoint PosRel (pParent->x(), pParent->y());
      QPoint PosAbs = pParent->mapToGlobal(PosRel);

      X = PosAbs.x() + pParent->width ()/2 - Dx/2;
      Y = PosAbs.y() + pParent->height()/2 - Dy/2;
   }
   else
   {
      X = (QApplication::desktop()->width  () - Dx) / 2;
      Y = (QApplication::desktop()->height () - Dy) / 2;
   }

   setGeometry (X, Y, Dx, Dy);
}

t_DlgMessage::~t_DlgMessage ()
{
   delete pOwn;
}

APIRET t_DlgMessage::Show (const QString &Title, const QString &Message, bool Monospaced, QWidget *pParent, Qt::WindowFlags Flags)
{
   t_DlgMessage *pDlg;

   pDlg = new t_DlgMessage (Title, Message, Monospaced, pParent, Flags);
   pDlg->setModal      (true);
   pDlg->show          ();
   pDlg->AdjustGeometry();
   pDlg->exec          ();
   delete pDlg;

   return NO_ERROR;
}

// ----------------------------------------------------------------
//  t_MessageBox can be used as a replacement for many QMessageBox
//  dialogs. The difference is, that t_MessageBox sets the font
//  The main parts of the code below has been copied from Qt
//  source code in src/gui/dialogs/qmessagebox.cpp
// ----------------------------------------------------------------

t_MessageBox::t_MessageBox (QWidget *pParent) :
   QMessageBox (pParent)
{
   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)
}

t_MessageBox::t_MessageBox (Icon icon, const QString & title, const QString & text, StandardButtons buttons, QWidget *parent, Qt::WindowFlags f) :
   QMessageBox (icon, title, text, buttons, parent, f)
{
   QTUTIL_SET_FONT (this, FONTOBJECT_MESSAGE_DIALOGS)
}

t_MessageBox::~t_MessageBox ()
{
}

QMessageBox::StandardButton t_MessageBox::showNewMessageBox(QWidget *pParent, QMessageBox::Icon Icon, const QString &Title, const QString &Text, QMessageBox::StandardButtons Buttons, QMessageBox::StandardButton DefaultButton)
{
   t_MessageBox       MsgBox (Icon, Title, Text, QMessageBox::NoButton, pParent);
   QDialogButtonBox *pButtonBox = MsgBox.findChild<QDialogButtonBox*>();

   Q_ASSERT (pButtonBox != nullptr);
   uint ButtonMask = QMessageBox::FirstButton;
   while (ButtonMask <= QMessageBox::LastButton)
   {
      uint CurrentButton = Buttons & ButtonMask;
      ButtonMask <<= 1;
      if (!CurrentButton)
         continue;
      QPushButton *pPushbutton = MsgBox.addButton ((QMessageBox::StandardButton)CurrentButton);
      if (MsgBox.defaultButton())  // Do we already have set the  default button?
         continue;
      if ((DefaultButton == QMessageBox::NoButton && pButtonBox->buttonRole(pPushbutton) == QDialogButtonBox::AcceptRole) ||
          (DefaultButton != QMessageBox::NoButton && CurrentButton == uint(DefaultButton)))
         MsgBox.setDefaultButton (pPushbutton);
   }
   if (MsgBox.exec() == -1)
      return QMessageBox::Cancel;
   return MsgBox.standardButton (MsgBox.clickedButton());
}

QMessageBox::StandardButton t_MessageBox::question(QWidget *parent, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton)
{
    return showNewMessageBox (parent, Question, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton t_MessageBox::information(QWidget *parent, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton)
{
   return showNewMessageBox(parent, Information, title, text, buttons,defaultButton);
}

QMessageBox::StandardButton t_MessageBox::warning(QWidget *parent, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton)
{
    return showNewMessageBox(parent, Warning, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton t_MessageBox::critical(QWidget *parent, const QString &title, const QString& text, StandardButtons buttons, StandardButton defaultButton)
{
    return showNewMessageBox(parent, Critical, title, text, buttons, defaultButton);
}
