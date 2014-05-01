/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef LAYERACLMENU_H
#define LAYERACLMENU_H

#include <QMenu>
#include <QList>

namespace net {
	class UserListModel;
}

namespace docks {

class LayerAclMenu : public QMenu
{
    Q_OBJECT
public:
	explicit LayerAclMenu(QWidget *parent = 0);

	void setUserList(net::UserListModel *model);
	void setAcl(bool lock, const QList<uint8_t> acl);

signals:
	/**
	 * @brief Layer Access Control List changed
	 *
	 * This signal includes the new exclusive access list.
	 * The list is empty if all users have access.
	 *
	 * @param lock general layer lock
	 * @param ids list of user IDs.
	 */
	void layerAclChange(bool lock, QList<uint8_t> ids);

private slots:
	void userClicked(QAction *useraction);
	void rowsInserted(const QModelIndex &parent, int start, int end);
	void rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int);
	void rowsRemoved(const QModelIndex &parent, int start, int end);

private:
	void addUser(int index);

	net::UserListModel *_model;
	QAction *_lock;
	QAction *_allusers;
	QList<QAction*> _users;
};

}

#endif // LAYERACLMENU_H
