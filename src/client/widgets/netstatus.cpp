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
#include <QProgressBar>

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

	// Data transfer progress (not always shown)
	_progress = new QProgressBar(this);
	_progress->setMaximumWidth(120);
	_progress->setSizePolicy(QSizePolicy());
	_progress->setTextVisible(false);
	_progress->hide();
	layout->addWidget(_progress);

	// Host address label
	_label = new QLabel(tr("not connected"), this);
	_label->setTextInteractionFlags(
			Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard
			);
	_label->setCursor(Qt::IBeamCursor);
	layout->addWidget(_label);

	// Action to copy address to clipboard
	_copyaction = new QAction(tr("Copy address to clipboard"), this);
	_copyaction->setEnabled(false);
	_label->addAction(_copyaction);
	_label->setContextMenuPolicy(Qt::ActionsContextMenu);
	connect(_copyaction,SIGNAL(triggered()),this,SLOT(copyAddress()));

	// Network connection status icon
	_icon = new QLabel(QString(), this);
	_icon->setPixmap(icon::network().pixmap(16,QIcon::Normal,QIcon::Off));
	_icon->setFixedSize(icon::network().actualSize(QSize(16,16)));
	layout->addWidget(_icon);

	// Popup label
	_popup = new PopupMessage(this);

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
	_address = address;
	_label->setText(tr("Connecting to %1...").arg(_address));
	_copyaction->setEnabled(true);
	message(_label->text());

	// reset statistics
	_recvbytes = 0;
	_sentbytes = 0;
	_online = true;
	updateIcon();
}

void NetStatus::loggedIn()
{
	_label->setText(tr("Host: %1").arg(_address));
	message(tr("Logged in!"));
}

/**
 * Set the label to indicate a lack of connection.
 * Context menu will be disabled.
 */
void NetStatus::hostDisconnected()
{
	_address = QString();
	_label->setText(tr("not connected"));

	_copyaction->setEnabled(false);
	message(tr("Disconnected"));
	_online = false;
	updateIcon();
}

void NetStatus::expectBytes(int count)
{
	_progress->reset();
	_progress->setMaximum(count);
	_progress->show();
}

void NetStatus::bytesReceived(int count)
{
	// TODO show statistics
	_recvbytes += count;
	if(!(_activity & 2)) {
		_activity |= 2;
		updateIcon();
	}
	if(_progress->isVisible()) {
		int val = _progress->value() + count;
		if(val>_progress->maximum())
			_progress->hide();
		else
			_progress->setValue(val);
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
	QApplication::clipboard()->setText(_address);
	// Put address also in selection buffer so it can be pasted with
	// a middle mouse click where supported.
	QApplication::clipboard()->setText(_address, QClipboard::Selection);
}

void NetStatus::join(const QString& user)
{
	message(tr("<b>%1</b> joined").arg(user));
}

void NetStatus::leave(const QString& user)
{
	message(tr("<b>%1</b> left").arg(user));
}

#if 0
void NetStatus::kicked(const network::User& user)
{
	message(tr("<b>%1</b> was kicked by session owner").arg(user.name()));
}
#endif

void NetStatus::message(const QString& msg)
{
	_popup->setMessage(msg);
	_popup->popupAt(mapToGlobal(_icon->pos() +
				QPoint(_icon->width()/2, _icon->height()/2)));
	emit statusMessage(msg);
}

}

