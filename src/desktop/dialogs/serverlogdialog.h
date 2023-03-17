/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2021 Calle Laakkonen

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
#ifndef SERVERLOGDIALOG_H
#define SERVERLOGDIALOG_H

#include <QDialog>

class Ui_ServerLogDialog;

class QSortFilterProxyModel;
class QItemSelection;
class QAbstractItemModel;

namespace canvas {
	class UserListModel;
}

namespace drawdance {
	class Message;
}

namespace dialogs {

class ServerLogDialog final : public QDialog
{
	Q_OBJECT
public:
	ServerLogDialog(QWidget *parent=nullptr);
	~ServerLogDialog() override;

	void setModel(QAbstractItemModel *model);
	void setUserList(canvas::UserListModel *userlist);

public slots:
	void setOperatorMode(bool op);

signals:
	void inspectModeChanged(int contextId);
	void inspectModeStopped();
	void opCommand(const drawdance::Message &msg);

protected:
	void hideEvent(QHideEvent *event) override;

private slots:
	void userSelected(const QItemSelection &selected);

	void setInspectMode(bool inspect);
	void kickSelected();
	void banSelected();
	void undoSelected();
	void redoSelected();

private:
	Ui_ServerLogDialog *m_ui;
	QSortFilterProxyModel *m_eventlogProxy;

	QSortFilterProxyModel *m_userlistProxy;
	canvas::UserListModel *m_userlist;

	bool m_opMode;

	uint8_t selectedUserId() const;
};

}

#endif

