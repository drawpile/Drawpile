/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QMenu>

#include "hostlabel.h"

namespace widgets {

HostLabel::HostLabel(QWidget *parent)
	:QLabel(QString(), parent)
{
	setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
	setCursor(Qt::IBeamCursor);
	menu_ = new QMenu(this);
	QAction *copy = menu_->addAction(tr("Copy address to clipboard"));
	connect(copy,SIGNAL(triggered()),this,SLOT(copyAddress()));
	menu_->setDefaultAction(copy);
	disconnect();
}

/**
 * Set the label to display the address.
 * A context menu to copy the address to clipboard will be enabled.
 * @param address the address to display
 */
void HostLabel::setAddress(const QString& address)
{
	address_ = address;
	setText(tr("Host: %1").arg(address_));
	menu_->defaultAction()->setEnabled(true);
}

/**
 * Set the label to indicate a lack of connection.
 * Context menu will be disabled.
 */
void HostLabel::disconnect()
{
	address_ = QString();
	setText(tr("not connected"));
	menu_->defaultAction()->setEnabled(false);

}

/**
 * Copy the current address to clipboard.
 * Should not be called if disconnected.
 */
void HostLabel::copyAddress()
{
	QApplication::clipboard()->setText(address_);
	// Put address also in selection buffer so it can be pasted with
	// a middle mouse click where supported.
	QApplication::clipboard()->setText(address_, QClipboard::Selection);
}

void HostLabel::contextMenuEvent(QContextMenuEvent *event)
{
	menu_->popup(event->globalPos());
}

}

