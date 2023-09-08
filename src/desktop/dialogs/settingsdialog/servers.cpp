// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/addserverdialog.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/servers.h"
#include "desktop/filewrangler.h"
#include "desktop/settings.h"
#include "desktop/utils/listserverdelegate.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/utils/certificatestoremodel.h"
#include "libclient/utils/listservermodel.h"
#include "libshared/util/paths.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSslCertificate>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>
#include <optional>

namespace dialogs {
namespace settingsdialog {

Servers::Servers(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);

	initListingServers(settings, layout);
	layout->addSpacing(6);
	initKnownHosts(layout);
}

void Servers::initKnownHosts(QVBoxLayout *form)
{
	auto *knownHostsLabel = new QLabel(tr("Known hosts:"));
	form->addWidget(knownHostsLabel);

	auto *knownHosts = new QListView;
	form->addWidget(knownHosts, 1);
	knownHostsLabel->setBuddy(knownHosts);
	knownHosts->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	knownHosts->setAlternatingRowColors(true);
	knownHosts->setFocusPolicy(Qt::StrongFocus);
	knownHosts->setSelectionMode(QAbstractItemView::ExtendedSelection);
	auto *knownHostsModel = new CertificateStoreModel(this);
	knownHosts->setModel(knownHostsModel);

	auto *actions = listActions(knownHosts,
		tr("Import trusted certificate…"),
		[=] { importCertificates(knownHostsModel); },

		tr("Remove selected hosts…"),
		makeDefaultDeleter(this, knownHosts,
			tr("Remove known hosts"),
			QT_TR_N_NOOP("Really remove %n known host(s)?")
		)
	);

	actions->addStretch();

	auto *trustButton = new QPushButton(tr("Trust selected hosts"));
	trustButton->setAutoDefault(false);
	trustButton->setEnabled(knownHosts->selectionModel()->hasSelection());
	connect(knownHosts->selectionModel(), &QItemSelectionModel::selectionChanged, trustButton, [=] {
		trustButton->setEnabled(knownHosts->selectionModel()->hasSelection());
	});
	connect(trustButton, &QPushButton::clicked, knownHosts, [=] {
		trustCertificates(
			knownHostsModel,
			knownHosts->selectionModel()->selectedIndexes()
		);
	});
	actions->addWidget(trustButton);

	connect(knownHosts, &QListView::doubleClicked, this, [=](const QModelIndex &index) {
		viewCertificate(knownHostsModel, index);
	});

	form->addLayout(actions);
}

void Servers::initListingServers(desktop::settings::Settings &settings, QVBoxLayout *form)
{
	auto *serversLabel = new QLabel(tr("List servers:"));
	form->addWidget(serversLabel);

	auto *servers = new QListView;
	serversLabel->setBuddy(servers);
	auto *serversModel = new sessionlisting::ListServerModel(settings, true, this);
	servers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	servers->setModel(serversModel);
	servers->setItemDelegate(new sessionlisting::ListServerDelegate(this));
	servers->setAlternatingRowColors(true);
	servers->setFocusPolicy(Qt::StrongFocus);
	servers->setSelectionMode(QAbstractItemView::ExtendedSelection);
	utils::initKineticScrolling(servers);
	form->addWidget(servers, 1);
	form->addLayout(listActions(servers,
		tr("Add list servers…"),
		[=] { addListServer(serversModel); },

		tr("Remove selected list servers…"),
		makeDefaultDeleter(this, servers,
			tr("Remove list servers"),
			QT_TR_N_NOOP("Really remove %n list server(s)?")
		),

		tr("Move up"),
		[=] { moveListServer(serversModel, servers->selectionModel(), -1); },

		tr("Move down"),
		[=] { moveListServer(serversModel, servers->selectionModel(), 1); }
	));
}

void Servers::addListServer(sessionlisting::ListServerModel *model)
{
	AddServerDialog *dialog = new AddServerDialog(this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setListServerModel(model);
	dialog->show();
}

void Servers::moveListServer(
	sessionlisting::ListServerModel *model, QItemSelectionModel *selectionModel,
	int offset)
{
	const QModelIndexList selectedRows = selectionModel->selectedRows();
	if(selectedRows.size() == 1) {
		int sourceRow = selectedRows.first().row();
		if(model->moveServer(sourceRow, sourceRow + offset)) {
			model->submit();
		}
	}
}

static bool askToContinue(const QString &title, const QString &message, QWidget *parent)
{
	QMessageBox box(
		QMessageBox::Warning,
		title,
		message,
		QMessageBox::Cancel,
		parent
	);
	const auto *ok = box.addButton(Servers::tr("Continue"), QMessageBox::AcceptRole);
	box.setDefaultButton(QMessageBox::Cancel);
	box.setWindowModality(Qt::WindowModal);
	box.exec();
	return box.clickedButton() == ok;
}

void Servers::importCertificates(CertificateStoreModel *model)
{
	const auto title = tr("Import trusted certificates");

	const auto paths = FileWrangler(this).getImportCertificatePaths(title);

	for (const auto &path : paths) {
		auto [ index, error ] = model->addCertificate(path, true);
		if (!error.isEmpty() && !askToContinue(title, error, this)) {
			model->revert();
			return;
		}
	}
	model->submit();
}

void Servers::trustCertificates(CertificateStoreModel *model, const QModelIndexList &indexes)
{
	for (const auto &index : indexes) {
		model->setData(index, true, CertificateStoreModel::TrustedRole);
	}
	if (!model->submit()) {
		execWarning(
			tr("Trust selected hosts"),
			tr("Could not save changes to known hosts: %1").arg(model->lastError()),
			this
		);
	}
}

void Servers::viewCertificate(CertificateStoreModel *model, const QModelIndex &index)
{
	const auto host = model->data(index, Qt::DisplayRole).toString();
	const auto cert = model->certificate(index);
	if (!cert) {
		return;
	}
	auto *cv = new CertificateView(host, *cert, this);
	cv->setWindowModality(Qt::WindowModal);
	cv->setAttribute(Qt::WA_DeleteOnClose);
	cv->show();
}

} // namespace settingsdialog
} // namespace dialogs
