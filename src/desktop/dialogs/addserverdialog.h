// SPDX-License-Identifier: GPL-3.0-or-later

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

