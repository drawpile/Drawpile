// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SERVERS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SERVERS_H
#include <QModelIndexList>
#include <QWidget>

class CertificateStoreModel;
class QDir;
class QItemSelectionModel;
class QListView;
class QModelIndex;
class QSslCertificate;
class QVBoxLayout;

namespace config {
class Config;
}

namespace sessionlisting {
class ListServerModel;
}

namespace dialogs {
namespace settingsdialog {

class Servers final : public QWidget {
	Q_OBJECT
public:
	Servers(config::Config *cfg, bool singleSession, QWidget *parent = nullptr);

private:
	void initListingServers(config::Config *cfg, QVBoxLayout *layout);

	void addListServer(sessionlisting::ListServerModel *model);

	void moveListServer(
		sessionlisting::ListServerModel *model,
		QItemSelectionModel *selectionModel, int offset);

#ifndef __EMSCRIPTEN__
	void initKnownHosts(QVBoxLayout *layout);

	void importCertificates(CertificateStoreModel *model);

	bool askToContinue(const QString &title, const QString &message);

	void pinCertificates(
		CertificateStoreModel *model, const QModelIndexList &indexes, bool pin);

	void
	viewCertificate(CertificateStoreModel *model, const QModelIndex &index);
#endif
};

}
}

#endif
