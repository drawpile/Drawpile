/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2018 Calle Laakkonen

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
	m_ui = new Ui_UserBox;
	m_ui->setupUi(this);

	m_ui->userlist->setItemDelegate(new UserListDelegate(this));
	setOperatorMode(false);

	m_ui->userlist->setSelectionMode(QListView::SingleSelection);

	connect(m_ui->lockButton, &QAbstractButton::clicked, this, &UserList::lockSelected);
	connect(m_ui->kickButton, &QAbstractButton::clicked, this, &UserList::kickSelected);
	connect(m_ui->banButton, &QAbstractButton::clicked, this, &UserList::kickBanSelected);
	connect(m_ui->undoButton, &QAbstractButton::clicked, this, &UserList::undoSelected);
	connect(m_ui->redoButton, &QAbstractButton::clicked, this, &UserList::redoSelected);
	connect(m_ui->opButton, &QAbstractButton::clicked, this, &UserList::opSelected);
	connect(m_ui->muteButton, &QAbstractButton::clicked, this, &UserList::muteSelected);
}

void UserList::setOperatorMode(bool op)
{
	m_ui->lockButton->setEnabled(op);
	m_ui->kickButton->setEnabled(op);
	m_ui->banButton->setEnabled(op);
	m_ui->undoButton->setEnabled(op);
	m_ui->redoButton->setEnabled(op);
	m_ui->opButton->setEnabled(op);
	m_ui->muteButton->setEnabled(op);
}

void UserList::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	m_ui->userlist->setModel(m_canvas->userlist());

	connect(m_canvas->userlist(), &canvas::UserListModel::dataChanged, this, &UserList::dataChanged);
	connect(m_canvas->aclFilter(), &canvas::AclFilter::localOpChanged, this, &UserList::opPrivilegeChanged);
	connect(m_ui->userlist->selectionModel(), &QItemSelectionModel::selectionChanged, this, &UserList::selectionChanged);
}

QModelIndex UserList::currentSelection()
{
	QModelIndexList sel = m_ui->userlist->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return QModelIndex();
	return sel.first();
}

void UserList::lockSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		const int id = idx.data().value<canvas::User>().id;
		bool lock = m_ui->lockButton->isChecked();

		emit opCommand(m_canvas->userlist()->getLockUserCommand(m_canvas->localUserId(), id, lock));
	}
}

void UserList::kickSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		emit opCommand(net::command::kick(idx.data().value<canvas::User>().id, false));
	}
}

void UserList::kickBanSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		emit opCommand(net::command::kick(idx.data().value<canvas::User>().id, true));
	}
}

void UserList::muteSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		const int id = idx.data().value<canvas::User>().id;
		const bool mute = m_ui->muteButton->isChecked();
		emit opCommand(net::command::mute(id, mute));
	}
}

void UserList::undoSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		emit opCommand(protocol::MessagePtr(new protocol::Undo(m_canvas->localUserId(), idx.data().value<canvas::User>().id, false)));
}

void UserList::redoSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid())
		emit opCommand(protocol::MessagePtr(new protocol::Undo(m_canvas->localUserId(), idx.data().value<canvas::User>().id, true)));
}

void UserList::opSelected()
{
	QModelIndex idx = currentSelection();
	if(idx.isValid()) {
		const int id = idx.data().value<canvas::User>().id;
		bool op = m_ui->opButton->isChecked();

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
		m_ui->lockButton->setChecked(user.isLocked);
		m_ui->opButton->setChecked(user.isOperator);
		m_ui->muteButton->setChecked(user.isMuted);
		if(user.isLocal) {
			m_ui->kickButton->setEnabled(false);
			m_ui->banButton->setEnabled(false);
			m_ui->opButton->setEnabled(false);
		}
	}
}

UserListDelegate::UserListDelegate(QObject *parent)
	: QItemDelegate(parent),
	  m_lockicon(icon::fromTheme("object-locked").pixmap(16, 16)),
	  m_opicon(icon::fromTheme("irc-operator").pixmap(16, 16)),
	  m_muteicon(icon::fromTheme("irc-unvoice").pixmap(16, 16)),
	  m_authicon(icon::fromTheme("im-user").pixmap(16, 16))
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
	const QSize locksize = m_lockicon.size();
	{
		QFontMetrics fm(opt.font);
		if(user.isMod) {
			const QString modmsg = QStringLiteral("MOD");
			opt.font.setBold(true);
			drawDisplay(painter, opt, textrect, modmsg);
			opt.font.setBold(false);
			textrect.moveLeft(textrect.left() + qMax(m_opicon.width()*2, fm.width(modmsg)) + 2);

		} else {
			if(user.isAuth)
				painter->drawPixmap(QRect(textrect.topLeft(), m_authicon.size()), m_authicon);

			textrect.moveLeft(textrect.left() + m_authicon.width() + 2);

			if(user.isOperator)
				painter->drawPixmap(QRect(textrect.topLeft(), m_opicon.size()), m_opicon);

			textrect.moveLeft(textrect.left() + m_opicon.width() + 2);
		}
	}

	if(user.isLocal)
		opt.font.setStyle(QFont::StyleItalic);

	drawDisplay(painter, opt, textrect, user.name);

	// Lock indicator
	if(user.isLocked)
		painter->drawPixmap(
			opt.rect.topRight()-QPoint(locksize.width(), -opt.rect.height()/2+locksize.height()/2),
			m_lockicon
		);

	// Mute indicator
	if(user.isMuted)
		painter->drawPixmap(
			opt.rect.topRight()-QPoint(locksize.width()*2, -opt.rect.height()/2+locksize.height()/2),
			m_muteicon
		);

	painter->restore();
}

QSize UserListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	const QSize iconsize = m_lockicon.size();
	if(size.height() < iconsize.height())
		size.setHeight(iconsize.height());
	return size;
}

}
