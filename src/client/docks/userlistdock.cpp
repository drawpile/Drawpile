/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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

#include <QListView>
#include <QPainter>
#include <QMouseEvent>

#include "docks/userlistdock.h"
#include "net/userlist.h"
#include "net/client.h"
#include "icons.h"

namespace widgets {

UserList::UserList(QWidget *parent)
	:QDockWidget(tr("Users"), parent)
{
	_list = new QListView(this);
	setWidget(_list);
}

void UserList::setClient(net::Client *client)
{
	_list->setModel(client->userlist());
	_list->setItemDelegate(new UserListDelegate(client, this));
}

UserListDelegate::UserListDelegate(net::Client *client, QObject *parent)
	: QItemDelegate(parent), _client(client)
{
}

void UserListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	bool op = _client->isOperator();
	const net::User user = index.data().value<net::User>();

	// Background
	drawBackground(painter, opt, index);

	// Lock button/indicator. This is shown even when not in admin mode.
	painter->drawPixmap(
			opt.rect.topLeft(),
			icon::lock().pixmap(
				16,
				QIcon::Normal,
				user.isLocked?QIcon::On:QIcon::Off)
			);

	// Name
	QRect textrect = opt.rect;
	const int kickwidth = icon::kick().actualSize(QSize(16,16)).width();
	textrect.setX(kickwidth + 5);

	if(user.isLocal)
		opt.font.setStyle(QFont::StyleItalic);

	if(user.isOperator)
		opt.palette.setColor(QPalette::Text, Qt::red);

	drawDisplay(painter, opt, textrect, user.name);

	// Kick button (only in operator mode)
	if(op && !user.isLocal)
		painter->drawPixmap(opt.rect.topRight()-QPoint(kickwidth,0),icon::kick().pixmap(16));

	painter->restore();
}

QSize UserListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	const QSize iconsize = icon::lock().actualSize(QSize(16,16));
	if(size.height() < iconsize.height())
		size.setHeight(iconsize.height());
	return size;
}

bool UserListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	if(_client->isOperator() && event->type() == QEvent::MouseButtonPress) {
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);

		if(me->button() == Qt::LeftButton) {
			const int btnwidth = icon::lock().actualSize(QSize(16,16)).width();

			const net::User user = index.data().value<net::User>();

			if(me->x() <= btnwidth) {
				// User pressed lock button
				_client->sendLockUser(user.id, !user.isLocked);
				return true;
			} else if(me->x() >= option.rect.width()-btnwidth) {
				if(user.isLocal==false) {
					// User pressed kick button (can't kick self though)
					_client->sendKickUser(user.id);
					return true;
				}
			}
		}
	}
	return QItemDelegate::editorEvent(event, model, option, index);
}


}
