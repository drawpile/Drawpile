/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2015 Calle Laakkonen

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

#include "widgets/userlistwidget.h"
#include "canvas/canvasmodel.h"
#include "canvas/userlist.h"
#include "canvas/aclfilter.h"
#include "utils/icon.h"
#include "net/commands.h"

#include "../shared/net/control.h"
#include "../shared/net/meta2.h"
#include "../shared/net/undo.h"

#include "widgets/groupedtoolbutton.h"
using widgets::GroupedToolButton;
#include "ui_userbox.h"

namespace widgets {

UserList::UserList(QWidget *parent)
	:QWidget(parent)
{
	_ui = new Ui_UserBox;
	_ui->setupUi(this);

	_ui->userlist->setItemDelegate(new UserListDelegate(this));
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

void UserList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	_ui->userlist->setModel(m_canvas->userlist());

	connect(m_canvas->userlist(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
	connect(m_canvas->aclFilter(), &canvas::AclFilter::localOpChanged, this, &UserList::opPrivilegeChanged);

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
	if(idx.isValid()) {
		const int id = idx.data().value<canvas::User>().id;
		bool lock = _ui->lockButton->isChecked();

		emit opCommand(m_canvas->userlist()->getLockUserCommand(m_canvas->localUserId(), id, lock));
	}
}

void UserList::kickSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		emit opCommand(net::command::kick(idx.data().value<canvas::User>().id));
	}
}

void UserList::undoSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		emit opCommand(protocol::MessagePtr(new protocol::Undo(m_canvas->localUserId(), idx.data().value<canvas::User>().id, 1)));
}

void UserList::redoSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		emit opCommand(protocol::MessagePtr(new protocol::Undo(m_canvas->localUserId(), idx.data().value<canvas::User>().id, -1)));
}

void UserList::opSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		const int id = idx.data().value<canvas::User>().id;
		bool op = _ui->opButton->isChecked();

		emit opCommand(m_canvas->userlist()->getOpUserCommand(m_canvas->localUserId(), id, op));
	}
}

void UserList::selectionChanged(const QItemSelection &selected)
{
	bool on = selected.count() > 0;

	setOperatorMode(on && m_canvas->aclFilter()->isLocalUserOperator() && m_canvas->isOnline());

	if(on) {
		QModelIndex cs = currentSelection();
		dataChanged(cs,cs);
	}
}

void UserList::opPrivilegeChanged()
{
	bool on = currentSelection().isValid();
	setOperatorMode(on && m_canvas->aclFilter()->isLocalUserOperator() && m_canvas->isOnline());
}

void UserList::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	const int myRow = currentSelection().row();
	if(topLeft.row() <= myRow && myRow <= bottomRight.row()) {
		const canvas::User &user = currentSelection().data().value<canvas::User>();
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

	const canvas::User user = index.data().value<canvas::User>();

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
