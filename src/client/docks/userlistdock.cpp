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

#include "ui_userbox.h"

namespace docks {

UserList::UserList(QWidget *parent)
	:QDockWidget(tr("Users"), parent)
{
	_ui = new Ui_UserBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);
	setOperatorMode(false);

	_ui->userlist->setSelectionMode(QListView::SingleSelection);

	connect(_ui->lockButton, SIGNAL(clicked()), this, SLOT(lockSelected()));
	connect(_ui->kickButton, SIGNAL(clicked()), this, SLOT(kickSelected()));
	connect(_ui->undoButton, SIGNAL(clicked()), this, SLOT(undoSelected()));
	connect(_ui->opButton, SIGNAL(clicked()), this, SLOT(opSelected()));
}

void UserList::setOperatorMode(bool op)
{
	_ui->lockButton->setEnabled(op);
	_ui->kickButton->setEnabled(op);
	_ui->undoButton->setEnabled(op);
	_ui->opButton->setEnabled(op);
}

void UserList::setClient(net::Client *client)
{
	_client = client;
	_ui->userlist->setModel(client->userlist());
	_ui->userlist->setItemDelegate(new UserListDelegate(this));

	connect(client->userlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(_ui->userlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));
}

QModelIndex UserList::currentSelection()
{
	QModelIndexList sel = _ui->userlist->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return QModelIndex();
	return sel.first();
}

void UserList::lockSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		_client->sendLockUser(idx.data().value<net::User>().id, _ui->lockButton->isChecked());
}

void UserList::kickSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		_client->sendKickUser(idx.data().value<net::User>().id);
}

void UserList::undoSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		_client->sendUndo(1, idx.data().value<net::User>().id);
}

void UserList::opSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		_client->sendOpUser(idx.data().value<net::User>().id, _ui->opButton->isChecked());
}

void UserList::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;

	setOperatorMode(on && _client->isOperator() && _client->isLoggedIn());

	if(on) {
		QModelIndex cs = currentSelection();
		dataChanged(cs,cs);
	}
}

void UserList::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	const int myRow = currentSelection().row();
	if(topLeft.row() <= myRow && myRow <= bottomRight.row()) {
		const net::User &user = currentSelection().data().value<net::User>();
		_ui->lockButton->setChecked(user.isLocked);
		_ui->opButton->setChecked(user.isOperator);
		if(user.isLocal) {
			_ui->kickButton->setEnabled(false);
			_ui->opButton->setEnabled(false);
		}
	}
}

UserListDelegate::UserListDelegate(QObject *parent)
	: QItemDelegate(parent),
	  _lockicon(QPixmap(":icons/lock_closed.png"))
{
}

void UserListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	const net::User user = index.data().value<net::User>();

	// Background
	drawBackground(painter, opt, index);

	// [OP] + Name
	QRect textrect = opt.rect;
	const QSize locksize = _lockicon.size();

	{
		opt.font.setBold(true);
		QFontMetrics fm(opt.font);
		QString opmsg("[OP]");

		if(user.isOperator)
			drawDisplay(painter, opt, textrect, "[OP]");

		textrect.moveLeft(fm.width(opmsg) + 5);
		opt.font.setBold(false);
	}

	if(user.isLocal)
		opt.font.setStyle(QFont::StyleItalic);

	drawDisplay(painter, opt, textrect, user.name);

	// Lock indicator
	if(user.isLocked)
		painter->drawPixmap(
			opt.rect.topRight()-QPoint(locksize.width(), -opt.rect.height()/2+locksize.height()/2),
			_lockicon
		);

	painter->restore();
}

QSize UserListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	const QSize iconsize = _lockicon.size();
	if(size.height() < iconsize.height())
		size.setHeight(iconsize.height());
	return size;
}

}
