// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/servers.h"
#include "desktop/dialogs/addserverdialog.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/filewrangler.h"
#include "desktop/settings.h"
#include "desktop/utils/listserverdelegate.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/utils/certificatestoremodel.h"
#include "libclient/utils/listservermodel.h"
#include "libshared/util/paths.h"
#include <QApplication>
#include <QDir>
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
#include <utility>

namespace dialogs {
namespace settingsdialog {

Servers::Servers(
	desktop::settings::Settings &settings, bool singleSession, QWidget *parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);
	if(!singleSession) {
		initListingServers(settings, layout);
	}
#ifndef __EMSCRIPTEN__
	utils::addFormSpacer(layout);
	initKnownHosts(layout);
#endif
}

void Servers::initListingServers(
	desktop::settings::Settings &settings, QVBoxLayout *form)
{
	auto *serversLabel = new QLabel(tr("List servers:"));
	form->addWidget(serversLabel);

	auto *servers = new QListView;
	serversLabel->setBuddy(servers);
	auto *serversModel =
		new sessionlisting::ListServerModel(settings, true, this);
	servers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	servers->setModel(serversModel);
	servers->setItemDelegate(new sessionlisting::ListServerDelegate(this));
	servers->setAlternatingRowColors(true);
	servers->setFocusPolicy(Qt::StrongFocus);
	servers->setSelectionMode(QAbstractItemView::ExtendedSelection);
	utils::bindKineticScrollingWith(
		servers, Qt::ScrollBarAlwaysOff, Qt ::ScrollBarAsNeeded);
	form->addWidget(servers, 1);
	form->addLayout(listActions(
		servers, tr("Add"), tr("Add list servers…"),
		[=] {
			addListServer(serversModel);
		},
		tr("Remove"), tr("Remove selected list servers…"),
		makeDefaultDeleter(
			this, servers, tr("Remove list servers"),
			QT_TR_N_NOOP("Really remove %n list server(s)?")),
		QString(), tr("Move up"),
		[=] {
			moveListServer(serversModel, servers->selectionModel(), -1);
		},
		QString(), tr("Move down"),
		[=] {
			moveListServer(serversModel, servers->selectionModel(), 1);
		}));
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

#ifndef __EMSCRIPTEN__
void Servers::initKnownHosts(QVBoxLayout *form)
{
	auto *knownHostsLabel = new QLabel(tr("Known hosts:"));
	form->addWidget(knownHostsLabel);

	auto *knownHosts = new QListView;
	form->addWidget(knownHosts, 1);
	knownHostsLabel->setBuddy(knownHosts);
	knownHosts->setAlternatingRowColors(true);
	knownHosts->setFocusPolicy(Qt::StrongFocus);
	knownHosts->setSelectionMode(QAbstractItemView::ExtendedSelection);
	utils::bindKineticScrollingWith(
		knownHosts, Qt::ScrollBarAlwaysOff, Qt ::ScrollBarAsNeeded);
	auto *knownHostsModel = new CertificateStoreModel(this);
	knownHosts->setModel(knownHostsModel);

	auto *actions = listActions(
		knownHosts, tr("Add"), tr("Import trusted certificate…"),
		[=] {
			importCertificates(knownHostsModel);
		},

		tr("Remove"), tr("Remove selected hosts…"),
		makeDefaultDeleter(
			this, knownHosts, tr("Remove known hosts"),
			QT_TR_N_NOOP("Really remove %n known host(s)?")));

	actions->addStretch();

	widgets::GroupedToolButton *pinButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	//: This refers to "certificate pinning", a technical term.
	pinButton->setText(tr("Pin"));
	pinButton->setToolTip(tr("Pin selected certificates"));
	pinButton->setEnabled(knownHosts->selectionModel()->hasSelection());
	connect(
		knownHosts->selectionModel(), &QItemSelectionModel::selectionChanged,
		pinButton, [=] {
			pinButton->setEnabled(knownHosts->selectionModel()->hasSelection());
		});
	connect(pinButton, &QPushButton::clicked, knownHosts, [=] {
		pinCertificates(
			knownHostsModel, knownHosts->selectionModel()->selectedIndexes(),
			true);
	});
	actions->addWidget(pinButton);

	widgets::GroupedToolButton *unpinButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	//: This refers to "certificate pinning", a technical term.
	unpinButton->setText(tr("Unpin"));
	unpinButton->setToolTip(tr("Unpin selected certificates"));
	unpinButton->setEnabled(knownHosts->selectionModel()->hasSelection());
	connect(
		knownHosts->selectionModel(), &QItemSelectionModel::selectionChanged,
		unpinButton, [=] {
			unpinButton->setEnabled(
				knownHosts->selectionModel()->hasSelection());
		});
	connect(unpinButton, &QPushButton::clicked, knownHosts, [=] {
		pinCertificates(
			knownHostsModel, knownHosts->selectionModel()->selectedIndexes(),
			false);
	});
	actions->addWidget(unpinButton);

	connect(
		knownHosts, &QListView::doubleClicked, this,
		[=](const QModelIndex &index) {
			viewCertificate(knownHostsModel, index);
		});

	form->addLayout(actions);
}

static bool
askToContinue(const QString &title, const QString &message, QWidget *parent)
{
	QMessageBox box(
		QMessageBox::Warning, title, message, QMessageBox::Cancel, parent);
	const auto *ok =
		box.addButton(Servers::tr("Continue"), QMessageBox::AcceptRole);
	box.setDefaultButton(QMessageBox::Cancel);
#	ifndef __EMSCRIPTEN__
	box.setWindowModality(Qt::WindowModal);
#	endif
	box.exec();
	return box.clickedButton() == ok;
}

void Servers::importCertificates(CertificateStoreModel *model)
{
	const auto title = tr("Import certificates");

	const auto paths = FileWrangler(this).getImportCertificatePaths(title);

	for(const auto &path : paths) {
		auto [index, error] = model->addCertificate(path, true);
		if(!error.isEmpty() && !askToContinue(title, error, this)) {
			model->revert();
			return;
		}
	}
	model->submit();
}

void Servers::pinCertificates(
	CertificateStoreModel *model, const QModelIndexList &indexes, bool pin)
{
	for(const auto &index : indexes) {
		model->setData(index, pin, CertificateStoreModel::TrustedRole);
	}
	if(!model->submit()) {
		execWarning(
			pin ? tr("Pin selected certificates")
				: tr("Unpin selected certificates"),
			tr("Could not save changes to known hosts: %1")
				.arg(model->lastError()),
			this);
	}
}

void Servers::viewCertificate(
	CertificateStoreModel *model, const QModelIndex &index)
{
	const auto host = model->data(index, Qt::DisplayRole).toString();
	const auto cert = model->certificate(index);
	if(!cert) {
		return;
	}
	auto *cv = new CertificateView(host, *cert, this);
	cv->setWindowModality(Qt::WindowModal);
	cv->setAttribute(Qt::WA_DeleteOnClose);
	cv->show();
}
#endif

} // namespace settingsdialog
} // namespace dialogs
