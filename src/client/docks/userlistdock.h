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
#ifndef USERLISTWIDGET_H
#define USERLISTWIDGET_H

#include <QDockWidget>
#include <QItemDelegate>

class Ui_UserBox;

namespace net {
	class Client;
}

namespace docks {

/**
 * @brief User list window
 * A dock widget that displays a list of users, with session administration
 * controls.
 */
class UserList: public QDockWidget
{
Q_OBJECT
public:
	UserList(QWidget *parent=0);

	void setClient(net::Client *client);

public slots:
	void opPrivilegeChanged();

private slots:
	void lockSelected();
	void kickSelected();
	void opSelected();
	void undoSelected();
	void redoSelected();

	void dataChanged(const QModelIndex &topLeft, const QModelIndex & bottomRight);
	void selectionChanged(const QItemSelection &selected);

private:
	void setOperatorMode(bool op);
	QModelIndex currentSelection();

	Ui_UserBox *_ui;
	net::Client *_client;
};

/**
 * A delegate to display a session user with status icons
 */
class UserListDelegate : public QItemDelegate {
Q_OBJECT
public:
	UserListDelegate(QObject *parent=0);

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const;

private:
	QPixmap _lockicon;
	QPixmap _opicon;
};


}

#endif
