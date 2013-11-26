/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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

#include <QAction>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>

#include "netstatus.h"
#include "popupmessage.h"
#include "icons.h"

namespace widgets {

NetStatus::NetStatus(QWidget *parent)
	: QWidget(parent), _sentbytes(0), _recvbytes(0), _activity(0)
{
	setMinimumHeight(16+2);

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setMargin(1);
	layout->setSpacing(4);

	// Host address label
	label_ = new QLabel(tr("not connected"), this);
	label_->setTextInteractionFlags(
			Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard
			);
	label_->setCursor(Qt::IBeamCursor);
	layout->addWidget(label_);

	// Action to copy address to clipboard
	copyaction_ = new QAction(tr("Copy address to clipboard"), this);
	copyaction_->setEnabled(false);
	label_->addAction(copyaction_);
	label_->setContextMenuPolicy(Qt::ActionsContextMenu);
	connect(copyaction_,SIGNAL(triggered()),this,SLOT(copyAddress()));

	// Network connection status icon
	_icon = new QLabel(QString(), this);
	_icon->setPixmap(icon::network().pixmap(16,QIcon::Normal,QIcon::Off));
	_icon->setFixedSize(icon::network().actualSize(QSize(16,16)));
	layout->addWidget(_icon);

	// Popup label
	popup_ = new PopupMessage(this);

	// Timer for activity update
	_timer = new QTimer(this);
	_timer->setSingleShot(true);
	connect(_timer, SIGNAL(timeout()), this, SLOT(updateStats()));
}

/**
 * Set the label to display the address.
 * A context menu to copy the address to clipboard will be enabled.
 * @param address the address to display
 */
void NetStatus::connectingToHost(const QString& address)
{
	address_ = address;
	label_->setText(tr("Connecting to %1...").arg(address_));
	copyaction_->setEnabled(true);
	message(label_->text());

	// reset statistics
	_recvbytes = 0;
	_sentbytes = 0;
	_online = true;
	updateIcon();
}

void NetStatus::loggedIn()
{
	label_->setText(tr("Host: %1").arg(address_));
	message(tr("Logged in!"));
}

/**
 * Set the label to indicate a lack of connection.
 * Context menu will be disabled.
 */
void NetStatus::hostDisconnected()
{
	address_ = QString();
	label_->setText(tr("not connected"));

	copyaction_->setEnabled(false);
	message(tr("Disconnected"));
	_online = false;
	updateIcon();
}

void NetStatus::bytesReceived(int count)
{
	// TODO show statistics
	_recvbytes += count;
	if(!(_activity & 2)) {
		_activity |= 2;
		updateIcon();
	}
	_timer->start(500);
}

void NetStatus::bytesSent(int count)
{
	// TODO show statistics
	_sentbytes += count;
	if(!(_activity & 1)) {
		_activity |= 1;
		updateIcon();
	}
	_timer->start(500);
}

void NetStatus::updateStats()
{
	_activity = 0;
	updateIcon();
}

void NetStatus::updateIcon()
{
	if(_online) {
		switch(_activity) {
		case 0: // idle
			_icon->setPixmap(icon::network().pixmap(16,QIcon::Normal,QIcon::On));
			break;
		case 1: // transmit
			_icon->setPixmap(icon::network_transmit().pixmap(16));
			break;
		case 2: // receive
			_icon->setPixmap(icon::network_receive().pixmap(16));
			break;
		case 3: // transmit & receive
			_icon->setPixmap(icon::network_transmit_receive().pixmap(16));
			break;
		}
	} else {
		_icon->setPixmap(icon::network().pixmap(16,QIcon::Normal,QIcon::Off));
	}
}

/**
 * Copy the current address to clipboard.
 * Should not be called if disconnected.
 */
void NetStatus::copyAddress()
{
	QApplication::clipboard()->setText(address_);
	// Put address also in selection buffer so it can be pasted with
	// a middle mouse click where supported.
	QApplication::clipboard()->setText(address_, QClipboard::Selection);
}

#if 0
void NetStatus::join(const network::User& user)
{
	message(tr("<b>%1</b> has joined").arg(user.name()));
}

void NetStatus::leave(const network::User& user)
{
	message(tr("<b>%1</b> has left").arg(user.name()));
}

void NetStatus::kicked(const network::User& user)
{
	message(tr("<b>%1</b> was kicked by session owner").arg(user.name()));
}

void NetStatus::lock(const QString& reason)
{
	message(tr("Board locked (%1)").arg(reason));
}

void NetStatus::unlock()
{
	message(tr("Board unlocked"));
}

#endif

void NetStatus::message(const QString& msg)
{
	popup_->setMessage(msg);
	popup_->popupAt(mapToGlobal(_icon->pos() +
				QPoint(_icon->width()/2, _icon->height()/2)));
	emit statusMessage(msg);
}

}

