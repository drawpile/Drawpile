/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#include <QDebug>
#include <QStyleOptionViewItemV2>
#include <QPainter>
#include <QMouseEvent>

#include "userlistmodel.h"
#include "icons.h"

//Q_DECLARE_METATYPE(network::User)

UserListModel::UserListModel(QObject *parent)
	: QAbstractListModel(parent), session_(0)
{
}

void UserListModel::setSession(network::SessionState *session)
{
#if 0
	session_ = session;
	if(session_) {
		beginInsertRows(QModelIndex(),0,session_->userCount());
		users_ = session_->users();
		endInsertRows();
		connect(session_, SIGNAL(userJoined(int)), this, SLOT(addUser(int)));
		connect(session_, SIGNAL(userLeft(int)), this, SLOT(removeUser(int)));
		connect(session_, SIGNAL(userLocked(int,bool)), this, SLOT(updateUsers()));
	} else {
		beginRemoveRows(QModelIndex(), 0, users_.size());
		users_.clear();
		endRemoveRows();
	}
#endif
}

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
#if 0
	if(index.row() < 0 || index.row() >= users_.size())
		return QVariant();
	if(role == Qt::DisplayRole && session_)
		return QVariant::fromValue(session_->user(users_.at(index.row())));
#endif
	return QVariant();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
#if 0
	if(parent.isValid())
		return 0;
	return users_.count();
#else
	return 0;
#endif
}

void UserListModel::addUser(int id)
{
#if 0
	if(users_.contains(id))
		return;
	int pos=0;
	while(pos < users_.count()) {
		if(users_.at(pos) > id)
			break;
		++pos;
	}
	beginInsertRows(QModelIndex(),pos,pos);
	users_.insert(pos,id);
	endInsertRows();
#endif
}

void UserListModel::removeUser(int id)
{
#if 0
	const int pos = users_.indexOf(id);
	beginRemoveRows(QModelIndex(),pos,pos);
	users_.removeAt(pos);
	endRemoveRows();
#endif
}

void UserListModel::updateUsers()
{
	emit dataChanged(index(0,0), index(users_.count()-1,0));
}

UserListDelegate::UserListDelegate(QObject *parent)
	: QItemDelegate(parent), enableadmin_(false)
{
}

/**
 * If admin mode is enabled, lock and kick buttons are visible and active.
 * Note that enabling admin mode should not be enabled if the user is not
 * actually the session owner, as the server will automatically kick users
 * who try to use admin commands without permission.
 * @param enable if true, admin command buttons are visible
 */
void UserListDelegate::setAdminMode(bool enable)
{
	enableadmin_ = enable;
}

void UserListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
#if 0
	QStyleOptionViewItemV2 opt = setOptions(index, option);
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);

	const network::User user = index.data().value<network::User>();

	// Background
	drawBackground(painter, opt, index);

	// Lock button/indicator. This is shown even when not in admin mode.
	painter->drawPixmap(
			opt.rect.topLeft(),
			icon::lock().pixmap(
				16,
				QIcon::Normal,
				user.locked()?QIcon::On:QIcon::Off)
			);

	// Name
	QRect textrect = opt.rect;
	const int kickwidth = icon::kick().actualSize(QSize(16,16)).width();
	textrect.setX(kickwidth + 5);
	if(user.isLocal())
		opt.font.setStyle(QFont::StyleItalic);
	drawDisplay(painter, opt, textrect, user.name());

	// Kick button (only in admin mode and for nonadmin users)
	if(enableadmin_ && user.isOwner()==false)
		painter->drawPixmap(opt.rect.topRight()-QPoint(kickwidth,0),icon::kick().pixmap(16));
#endif
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
#if 0
	if(enableadmin_ && event->type() == QEvent::MouseButtonPress) {
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);

		const int btnwidth = icon::lock().actualSize(QSize(16,16)).width();

		network::User user = index.data().value<network::User>();
		if(me->x() <= btnwidth) {
			// User pressed lock button
			user.lock( !user.locked() );
			return true;
		} else if(me->x() >= option.rect.width()-btnwidth) {
			if(user.isOwner()==false) {
				// User pressed kick button (won't kick session owner though)
				user.kick();
				return true;
			}
		}
	}
#endif
	return QItemDelegate::editorEvent(event, model, option, index);
}

