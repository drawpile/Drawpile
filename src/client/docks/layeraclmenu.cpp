/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include "docks/layeraclmenu.h"
#include "net/userlist.h"

namespace docks {

LayerAclMenu::LayerAclMenu(QWidget *parent) :
    QMenu(parent)
{
	_lock = addAction(tr("Lock this layer"));
	_lock->setCheckable(true);

	addSection(tr("Exclusive access:"));

	_allusers = addAction(tr("Everyone can draw"));
	_allusers->setCheckable(true);
	_allusers->setChecked(true);

	connect(this, SIGNAL(triggered(QAction*)), this, SLOT(userClicked(QAction*)));
}

void LayerAclMenu::setUserList(net::UserListModel *model)
{
	_model = model;
	for(int i=0;i<model->rowCount();++i)
		addUser(i);

	connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
	connect(model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), this, SLOT(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
	connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(rowsRemoved(QModelIndex,int,int)));
}

void LayerAclMenu::addUser(int index)
{
	const net::User &user = _model->data(_model->index(index)).value<net::User>();
	QAction *userAction = new QAction(user.name, this);
	userAction->setCheckable(true);
	userAction->setProperty("userid", user.id);
	_users.append(userAction);
	addAction(userAction);
}

void LayerAclMenu::rowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent);
	Q_ASSERT(start == _users.count()); // new users are always added to the end of the list
	for(;start<=end;++start)
		addUser(start);
}

void LayerAclMenu::rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)
{
	// Users are never moved in the list
	qWarning("rowsMoved not implemented!");
}

void LayerAclMenu::rowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent);
	for(int i=start;i<=end;++i) {
		removeAction(_users.takeAt(start));
	}
}

void LayerAclMenu::userClicked(QAction *useraction)
{
	// Get exclusive user access list
	QList<uint8_t> exclusive;
	for(const QAction *a : _users) {
		if(a->isChecked())
			exclusive.append(a->property("userid").toInt());
	}

	if(useraction == _lock) {
		bool enable = !useraction->isChecked();

		_allusers->setEnabled(enable);
		foreach(QAction *a, _users)
			a->setEnabled(enable);

	} else if(useraction == _allusers) {
		// No user has exclusive access
		exclusive.clear();
		_allusers->setChecked(true);
		foreach(QAction *a, _users)
			a->setChecked(false);

	} else {
		// User exclusive access bit changed.
		_allusers->setChecked(exclusive.isEmpty());
	}

	// Send ACL update message
	emit layerAclChange(_lock->isChecked(), exclusive);
}

void LayerAclMenu::setAcl(bool lock, const QList<uint8_t> acl)
{
	_lock->setChecked(lock);

	_allusers->setEnabled(!lock);

	for(QAction *u : _users) {
		u->setChecked(false);
		u->setEnabled(!lock);
	}

	_allusers->setChecked(acl.isEmpty());
	if(!acl.isEmpty()) {
		for(uint8_t id : acl) {
			const QVariant qvid = id;
			for(QAction *u : _users) {
				if(u->property("userid") == qvid) {
					u->setChecked(true);
					break;
				}
			}
		}
	}
}

}
