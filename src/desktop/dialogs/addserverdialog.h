/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2020 Calle Laakkonen

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
#ifndef ADDSERVERDIALOG_H
#define ADDSERVERDIALOG_H

#include "libclient/net/sessionlistingmodel.h"

#include <QMessageBox>

namespace sessionlisting {
    class ListServerModel;
}

namespace dialogs {

class AddServerDialog final : public QMessageBox
{
	Q_OBJECT
public:
	explicit AddServerDialog(QWidget *parent=nullptr);

	void query(const QUrl &query);
	void setListServerModel(sessionlisting::ListServerModel *model);

signals:
	void serverAdded(const QString &name);

private slots:
	void onAddClicked();

private:
	void showError(const QString &errorMessage);
	void showSuccess();

	QUrl m_url;
	sessionlisting::ListServerInfo m_serverInfo;
	QImage m_favicon;
	sessionlisting::ListServerModel *m_servers;
};

}

#endif

