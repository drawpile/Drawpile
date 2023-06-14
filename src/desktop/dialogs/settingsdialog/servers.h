// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SERVERS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SERVERS_H

#include <QWidget>
#include <utility>

class CertificateStoreModel;
class QDir;
class QItemSelectionModel;
class QListView;
class QSslCertificate;
class QVBoxLayout;

namespace desktop { namespace settings { class Settings; } }
namespace sessionlisting { class ListServerModel; }

namespace dialogs {
namespace settingsdialog {

class Servers final : public QWidget {
	Q_OBJECT
public:
	Servers(desktop::settings::Settings &settings, QWidget *parent = nullptr);
private:
	void initKnownHosts(QVBoxLayout *layout);
	void initListingServers(desktop::settings::Settings &settings, QVBoxLayout *layout);

	void addListServer(sessionlisting::ListServerModel *model);
	void moveListServer(
		sessionlisting::ListServerModel *model,
		QItemSelectionModel *selectionModel, int offset);
	void importCertificates(CertificateStoreModel *model);
	void trustCertificates(CertificateStoreModel *model, const QModelIndexList &indexes);
	void viewCertificate(CertificateStoreModel *model, const QModelIndex &index);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
