// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/dialogs.h"
#include "desktop/dialogs/startdialog/host/files.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/utils/hostpresetmodel.h"
#include "libshared/util/paths.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

namespace {
static void migrateOldHostPresets(utils::StateDatabase &state)
{
	utils::HostPresetModel::migrateOldPresets(
		state, utils::paths::writablePath(QStringLiteral("permissions")));
}
}


ResetDialog::ResetDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(tr("Reset"));
	resize(400, 300);

	QVBoxLayout *layout = new QVBoxLayout(this);

	m_categories = new Categories(
		tr("Pick the sections you want to reset to their defaults:"));
	layout->addWidget(m_categories);
	for(QCheckBox *box : m_categories->boxes()) {
		box->setChecked(true);
		connect(box, &QCheckBox::clicked, this, &ResetDialog::updateOkButton);
	}

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &ResetDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &ResetDialog::reject);

	connect(
		this, &ResetDialog::accepted, this, &ResetDialog::emitResetRequests);
	updateOkButton();
}

void ResetDialog::updateOkButton()
{
	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	okButton->setEnabled(m_categories->isAnyCategoryChecked());
}

void ResetDialog::emitResetRequests()
{
	for(int i = 0; i < int(Categories::Count); ++i) {
		if(m_categories->isCategoryChecked(i)) {
			emit resetRequested(i, true, true, true);
		}
	}
}


LoadDialog::LoadDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(tr("Load Settings"));
	resize(400, 350);

	QVBoxLayout *layout = new QVBoxLayout(this);

	utils::StateDatabase &state = dpApp().state();
	migrateOldHostPresets(state);
	m_presetModel = new utils::HostPresetModel(state, this);

	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortRole(utils::HostPresetModel::SortRole);
	proxyModel->setSourceModel(m_presetModel);
	proxyModel->sort(0);

	m_presetCombo = new QComboBox;
	m_presetCombo->setModel(proxyModel);
	if(m_presetCombo->count() > 1) {
		m_presetCombo->setCurrentIndex(1);
	}
	layout->addWidget(m_presetCombo);
	connect(
		m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LoadDialog::updateCategories);
	connect(
		m_presetModel, &utils::HostPresetModel::rowsRemoved, this,
		&LoadDialog::updateCurrentCategories);
	connect(
		m_presetModel, &utils::HostPresetModel::dataChanged, m_presetCombo,
		QOverload<>::of(&QComboBox::update), Qt::QueuedConnection);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(buttonsLayout);

	m_renameButton =
		new QPushButton(QIcon::fromTheme("edit-rename"), tr("Rename"));
	buttonsLayout->addWidget(m_renameButton);
	connect(
		m_renameButton, &QPushButton::clicked, this,
		&LoadDialog::renameCurrentPreset);

	m_deleteButton =
		new QPushButton(QIcon::fromTheme("trash-empty"), tr("Delete"));
	buttonsLayout->addWidget(m_deleteButton);
	connect(
		m_deleteButton, &QPushButton::clicked, this,
		&LoadDialog::deleteCurrentPreset);

	utils::addFormSeparator(layout);

	m_categories = new Categories(
		tr("Pick the sections you want to load:"), true, true, true);
	layout->addWidget(m_categories);
	for(QCheckBox *box : m_categories->boxes()) {
		box->setChecked(true);
		connect(box, &QCheckBox::clicked, this, &LoadDialog::updateButtons);
	}

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &LoadDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &LoadDialog::reject);

	connect(this, &LoadDialog::accepted, this, &LoadDialog::emitLoadRequests);
	updateCategories(m_presetCombo->currentIndex());
}

void LoadDialog::updateCurrentCategories()
{
	updateCategories(m_presetCombo->currentIndex());
}

void LoadDialog::updateCategories(int i)
{
	if(m_presetCombo->itemData(i, utils::HostPresetModel::DefaultRole)
		   .toBool()) {
		for(QCheckBox *box : m_categories->boxes()) {
			box->setEnabled(true);
		}
		m_categories->replaceAnnouncementsBox()->setEnabled(true);
		m_categories->replaceAuthBox()->setEnabled(true);
		m_categories->replaceBansBox()->setEnabled(true);
	} else {
		int version =
			m_presetCombo->itemData(i, utils::HostPresetModel::VersionRole)
				.toInt();
		QJsonObject json =
			m_presetCombo->itemData(i, utils::HostPresetModel::DataRole)
				.toJsonObject();
		if(version != 1 || json.isEmpty()) {
			for(QCheckBox *box : m_categories->boxes()) {
				box->setEnabled(false);
			}
		} else {
			m_categories->sessionBox()->setEnabled(
				json.contains(QStringLiteral("session")));
			bool haveListing = json.contains(QStringLiteral("listing"));
			m_categories->listingBox()->setEnabled(haveListing);
			m_categories->replaceAnnouncementsBox()->setEnabled(
				haveListing &&
				json[QStringLiteral("listing")].toObject().contains(
					QStringLiteral("announcements")));
			m_categories->permissionsBox()->setEnabled(
				json.contains(QStringLiteral("permissions")));
			bool haveRoles = json.contains(QStringLiteral("roles"));
			m_categories->rolesBox()->setEnabled(haveRoles);
			m_categories->replaceAuthBox()->setEnabled(
				haveRoles && json[QStringLiteral("roles")].toObject().contains(
								 QStringLiteral("auth")));
			bool haveBans = json.contains(QStringLiteral("bans"));
			m_categories->bansBox()->setEnabled(haveBans);
			m_categories->replaceBansBox()->setEnabled(haveBans);
		}
	}
	updateButtons();
}

void LoadDialog::updateButtons()
{
	bool editable =
		!m_presetCombo->currentData(utils::HostPresetModel::DefaultRole)
			 .toBool();
	m_renameButton->setEnabled(editable);
	m_deleteButton->setEnabled(editable);
	m_categories->replaceAnnouncementsBox()->setVisible(
		m_categories->isCategoryChecked(Categories::Listing));
	m_categories->replaceAuthBox()->setVisible(
		m_categories->isCategoryChecked(Categories::Roles));
	m_categories->replaceBansBox()->setVisible(
		m_categories->isCategoryChecked(Categories::Bans));
	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	okButton->setEnabled(m_categories->isAnyCategoryChecked());
}

void LoadDialog::renameCurrentPreset()
{
	QString oldTitle =
		m_presetCombo->currentData(utils::HostPresetModel::TitleRole)
			.toString()
			.trimmed();
	QString newTitle = QInputDialog::getText(
						   this, tr("Rename"),
						   tr("Choose a new name for preset %1.").arg(oldTitle))
						   .trimmed();
	if(!newTitle.isEmpty() && newTitle != oldTitle) {
		int id =
			m_presetCombo->currentData(utils::HostPresetModel::IdRole).toInt();
		QSet<int> ids = m_presetModel->getPresetIdsByTitle(newTitle);
		ids.remove(id);
		if(ids.isEmpty()) {
			m_presetModel->renamePresetById(id, newTitle);
		} else {
			QMessageBox *box = utils::makeQuestion(
				this, tr("Conflict"),
				tr("Preset %1 already exists, do you want to replace it?")
					.arg(newTitle));
			box->button(QMessageBox::Yes)->setText(tr("Yes, replace"));
			box->button(QMessageBox::No)->setText(tr("No, keep"));
			connect(box, &QMessageBox::accepted, this, [this, id, newTitle] {
				m_presetModel->renamePresetById(id, newTitle);
			});
			box->show();
		}
	}
}

void LoadDialog::deleteCurrentPreset()
{
	QString title =
		m_presetCombo->currentData(utils::HostPresetModel::TitleRole)
			.toString()
			.trimmed();
	QMessageBox *box = utils::makeQuestion(
		this, tr("Delete"),
		tr("Are you sure you want to delete preset %1?").arg(title));
	box->button(QMessageBox::Yes)->setText(tr("Yes, delete"));
	box->button(QMessageBox::No)->setText(tr("No, keep"));
	connect(box, &QMessageBox::accepted, this, [this] {
		m_presetModel->deletePresetById(
			m_presetCombo->currentData(utils::HostPresetModel::IdRole).toInt());
	});
	box->show();
}

void LoadDialog::emitLoadRequests()
{
	if(m_presetCombo->currentData(utils::HostPresetModel::DefaultRole)
		   .toBool()) {
		for(int i = 0; i < int(Categories::Count); ++i) {
			if(m_categories->isCategoryChecked(i)) {
				emit resetRequested(
					i, m_categories->isReplaceAnnouncementsChecked(),
					m_categories->isReplaceAuthChecked(),
					m_categories->isReplaceBansChecked());
			}
		}
	} else {
		int version =
			m_presetCombo->currentData(utils::HostPresetModel::VersionRole)
				.toInt();
		if(version == 1) {
			QJsonObject json =
				m_presetCombo->currentData(utils::HostPresetModel::DataRole)
					.toJsonObject();
			emitLoadRequestsFrom(json);
		}
	}
}

void LoadDialog::emitLoadRequestsFrom(const QJsonObject &json)
{
	for(int i = 0; i < int(Categories::Count); ++i) {
		if(m_categories->isCategoryChecked(i)) {
			emit loadRequested(
				i, json[Categories::getCategoryName(i)].toObject(),
				m_categories->isReplaceAnnouncementsChecked(),
				m_categories->isReplaceAuthChecked(),
				m_categories->isReplaceBansChecked());
		}
	}
}


SaveDialog::SaveDialog(
	const QJsonObject &session, const QJsonObject &listing,
	const QJsonObject &permissions, const QJsonObject &roles,
	const QJsonObject &bans, QWidget *parent)
	: QDialog(parent)
	, m_categoryData({session, listing, permissions, roles, bans})
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(tr("Load Settings"));
	resize(400, 400);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QFormLayout *form = new QFormLayout;
	form->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(form);

	m_titleCombo = new QComboBox;
	m_titleCombo->setEditable(true);
	m_titleCombo->setInsertPolicy(QComboBox::NoInsert);
	loadExistingTitles();
	m_titleCombo->setEditText(QString());
	form->addRow(tr("Name:"), m_titleCombo);
	connect(
		m_titleCombo, &QComboBox::editTextChanged, this,
		&SaveDialog::updateOkButton);

	utils::addFormSeparator(layout);

	m_categories = new Categories(tr("Pick the sections you want to save:"));
	layout->addWidget(m_categories);
	for(int i = 0; i < int(Categories::Count); ++i) {
		QCheckBox *box = m_categories->boxes()[i];
		box->setChecked(true);
		connect(box, &QCheckBox::clicked, this, &SaveDialog::updateOkButton);
	}

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &SaveDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &SaveDialog::reject);

	updateOkButton();
}

void SaveDialog::accept()
{
	utils::StateDatabase &state = dpApp().state();
	migrateOldHostPresets(state);

	QString title = m_titleCombo->currentText().trimmed();
	if(!utils::HostPresetModel::getPresetIdsByTitleWith(state, title)
			.isEmpty()) {
		QMessageBox *box = utils::makeQuestion(
			this, tr("Save"),
			tr("Preset %1 already exists, do you want to replace it?")
				.arg(title));
		box->button(QMessageBox::Yes)->setText(tr("Yes, replace"));
		box->button(QMessageBox::No)->setText(tr("No, keep"));
		if(box->exec() != QMessageBox::Yes) {
			return;
		}
	}

	QJsonObject selectedData;
	for(int i = 0; i < int(Categories::Count); ++i) {
		if(m_categories->isCategoryChecked(i)) {
			QString key = Categories::getCategoryName(i);
			selectedData[key] = m_categoryData[i];
		}
	}

	state.tx([&](utils::StateDatabase::Query &qry) {
		return qry.exec(
				   QStringLiteral("delete from host_presets where title = ?"),
				   {title}) &&
			   qry.exec(
				   QStringLiteral(
					   "insert into host_presets (version, title, data) "
					   "values (?, ?, ?)"),
				   {1, title,
					QJsonDocument(selectedData)
						.toJson(QJsonDocument::Compact)});
	});

	QDialog::accept();
}

void SaveDialog::loadExistingTitles()
{
	utils::StateDatabase::Query qry = dpApp().state().query();
	if(qry.exec(QStringLiteral(
		   "select distinct title from host_presets order by title"))) {
		while(qry.next()) {
			m_titleCombo->addItem(qry.value(0).toString());
		}
	}
}

void SaveDialog::updateOkButton()
{
	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	okButton->setEnabled(
		!m_titleCombo->currentText().trimmed().isEmpty() &&
		m_categories->isAnyCategoryChecked());
}


ImportDialog::ImportDialog(const QJsonObject &importData, QWidget *parent)
	: QDialog(parent)
	, m_importData(importData)
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(tr("Import Settings"));
	resize(400, 300);

	QVBoxLayout *layout = new QVBoxLayout(this);

	m_categories = new Categories(
		tr("Pick the sections you want to import:"), true, true, true);
	layout->addWidget(m_categories);
	for(int i = 0; i < int(Categories::Count); ++i) {
		QCheckBox *box = m_categories->boxes()[i];
		QString key = Categories::getCategoryName(i);
		bool haveData = m_importData[key].isObject();
		box->setChecked(haveData);
		box->setEnabled(haveData);
		if(haveData) {
			connect(
				box, &QCheckBox::clicked, this, &ImportDialog::updateButtons);
		}
	}

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(
		m_buttons, &QDialogButtonBox::accepted, this, &ImportDialog::accept);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this, &ImportDialog::reject);

	connect(
		this, &ImportDialog::accepted, this, &ImportDialog::emitLoadRequests);
	updateButtons();
}

void ImportDialog::updateButtons()
{
	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	okButton->setEnabled(m_categories->isAnyCategoryChecked());
	m_categories->replaceAnnouncementsBox()->setVisible(
		m_categories->isCategoryChecked(Categories::Listing));
	m_categories->replaceAuthBox()->setVisible(
		m_categories->isCategoryChecked(Categories::Roles));
	m_categories->replaceBansBox()->setVisible(
		m_categories->isCategoryChecked(Categories::Bans));
}

void ImportDialog::emitLoadRequests()
{
	for(int i = 0; i < int(Categories::Count); ++i) {
		if(m_categories->isCategoryChecked(i)) {
			emit loadRequested(
				i, m_importData[Categories::getCategoryName(i)].toObject(),
				m_categories->isReplaceAnnouncementsChecked(),
				m_categories->isReplaceAuthChecked(),
				m_categories->isReplaceBansChecked());
		}
	}
}


ExportDialog::ExportDialog(
	const QJsonObject &session, const QJsonObject &listing,
	const QJsonObject &permissions, const QJsonObject &roles,
	const QJsonObject &bans, QWidget *parent)
	: QDialog(parent)
	, m_categoryData({session, listing, permissions, roles, bans})
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(tr("Export Settings"));
	resize(400, 300);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QFormLayout *form = new QFormLayout;
	form->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(form);

	m_categories = new Categories(tr("Pick the sections you want to export:"));
	layout->addWidget(m_categories);
	for(int i = 0; i < int(Categories::Count); ++i) {
		QCheckBox *box = m_categories->boxes()[i];
		box->setChecked(true);
		connect(box, &QCheckBox::clicked, this, &ExportDialog::updateOkButton);
	}

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(
		m_buttons, &QDialogButtonBox::accepted, this, &ExportDialog::accept);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this, &ExportDialog::reject);

	updateOkButton();
}

void ExportDialog::accept()
{
	QJsonObject selectedData;
	for(int i = 0; i < int(Categories::Count); ++i) {
		if(m_categories->isCategoryChecked(i)) {
			QString key = Categories::getCategoryName(i);
			selectedData[key] = m_categoryData[i];
		}
	}
	QString error;
	if(exportSessionSettings(this, selectedData, &error)) {
		QDialog::accept();
	} else if(!error.isEmpty()) {
		utils::showCritical(
			this, tr("Export Error"), tr("Could not export session settings."),
			error);
	}
}

void ExportDialog::updateOkButton()
{
	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	okButton->setEnabled(m_categories->isAnyCategoryChecked());
}

}
}
}
