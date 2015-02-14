/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QListView>
#include <QPainter>
#include <QMouseEvent>

#include "docks/userlistdock.h"
#include "docks/utils.h"
#include "net/userlist.h"
#include "net/client.h"
#include "utils/icon.h"

#include "widgets/groupedtoolbutton.h"
using widgets::GroupedToolButton;
#include "ui_userbox.h"

namespace docks {

UserList::UserList(QWidget *parent)
	:QDockWidget(tr("Users"), parent)
{
	_ui = new Ui_UserBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);

	setStyleSheet(defaultDockStylesheet());

	setOperatorMode(false);

	_ui->userlist->setSelectionMode(QListView::SingleSelection);

	connect(_ui->lockButton, SIGNAL(clicked()), this, SLOT(lockSelected()));
	connect(_ui->kickButton, SIGNAL(clicked()), this, SLOT(kickSelected()));
	connect(_ui->undoButton, SIGNAL(clicked()), this, SLOT(undoSelected()));
	connect(_ui->redoButton, SIGNAL(clicked()), this, SLOT(redoSelected()));
	connect(_ui->opButton, SIGNAL(clicked()), this, SLOT(opSelected()));
}

void UserList::setOperatorMode(bool op)
{
	_ui->lockButton->setEnabled(op);
	_ui->kickButton->setEnabled(op);
	_ui->undoButton->setEnabled(op);
	_ui->redoButton->setEnabled(op);
	_ui->opButton->setEnabled(op);
}

void UserList::setClient(net::Client *client)
{
	_client = client;
	_ui->userlist->setModel(client->userlist());
	_ui->userlist->setItemDelegate(new UserListDelegate(this));

	connect(client->userlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(_ui->userlist->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(selectionChanged(QItemSelection)));
	connect(client, SIGNAL(opPrivilegeChange(bool)), this, SLOT(opPrivilegeChanged()));
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

void UserList::redoSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		_client->sendUndo(-1, idx.data().value<net::User>().id);
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

void UserList::opPrivilegeChanged()
{
	bool on = currentSelection().isValid();
	setOperatorMode(on && _client->isOperator() && _client->isLoggedIn());
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
	  _lockicon(icon::fromTheme("object-locked").pixmap(16, 16)),
	  _opicon(icon::fromTheme("irc-operator").pixmap(16, 16))
{
}

void UserListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	const net::User user = index.data().value<net::User>();

	// Background
	drawBackground(painter, opt, index);

	// (auth) + [OP/MOD] + Name
	QRect textrect = opt.rect;
	const QSize locksize = _lockicon.size();
	{
		QFontMetrics fm(opt.font);
		QString authmsg = QStringLiteral("☆");
		if(user.isAuth)
			drawDisplay(painter, opt, textrect, authmsg);

		textrect.moveLeft(textrect.left() + fm.width(authmsg) + 2);

		QString modmsg = QStringLiteral("[MOD]");

		opt.font.setBold(true);
		if(user.isMod)
			drawDisplay(painter, opt, textrect, modmsg);
		else if(user.isOperator)
			painter->drawPixmap(QRect(textrect.topLeft(), _opicon.size()), _opicon);

		textrect.moveLeft(textrect.left() + fm.width(modmsg) + 7);
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
