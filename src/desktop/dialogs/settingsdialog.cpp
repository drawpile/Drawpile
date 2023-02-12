/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2021 Calle Laakkonen

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

#include "config.h" // for default port
#include "desktop/main.h"
#include "desktop/dialogs/settingsdialog.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/dialogs/avatarimport.h"
#include "desktop/dialogs/addserverdialog.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/widgets/keysequenceedit.h"
#include "desktop/scene/canvasviewmodifiers.h"
#include "libclient/utils/icon.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libclient/utils/listservermodel.h"
#include "desktop/utils/listserverdelegate.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libshared/listings/announcementapi.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"

#include "ui_settings.h"

#include <QSettings>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QItemEditorFactory>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QSslCertificate>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#include <QDebug>

using color_widgets::ColorWheel;

class KeySequenceEditFactory : public QItemEditorCreatorBase
{
public:
	QWidget *createWidget(QWidget *parent) const
	{
		return new widgets::KeySequenceEdit(parent);
	}

	QByteArray valuePropertyName() const
	{
		return "keySequence";
	}
};

namespace dialogs {

/**
 * Construct a settings dialog. The actions in the list should have
 * a "defaultshortcut" property for reset to default to work.
 *
 * @param actions list of customizeable actions (for shortcut editing)
 * @param parent parent widget
 */
SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	m_ui = new Ui_SettingsDialog;
	m_ui->setupUi(this);

	connect(m_ui->notificationVolume, &QSlider::valueChanged, [this](int val) {
		if(val>0)
			m_ui->volumeLabel->setText(QString::number(val) + "%");
		else
			m_ui->volumeLabel->setText(tr("off", "notifications sounds"));
	});

	// Get available languages
	m_ui->languageBox->addItem(tr("Default"), QString());
	m_ui->languageBox->addItem(QStringLiteral("English"), QStringLiteral("en"));

	const QLocale localeC = QLocale::c();
	QStringList locales;
	for(const QString &datapath : utils::paths::dataPaths()) {
		const QStringList files = QDir(datapath + "/i18n").entryList(QStringList("drawpile_*.qm"), QDir::Files, QDir::Name);
		for(const QString &file : files) {
			QString localename = file.mid(9, file.length() - 3 - 9);
			QLocale locale(localename);
			if(locale != localeC && !locales.contains(localename)) {
				locales << localename;
				m_ui->languageBox->addItem(locale.nativeLanguageName(), localename);
			}
		}
	}

	// Night mode support needs Qt 5.12 on macOS
#ifdef Q_OS_MACOS
	m_ui->formLayout_2->removeRow(m_ui->themeChoice);
#endif

	// Hide Windows specific stuff on other platforms
#if !defined(Q_OS_WIN) || !defined(KIS_TABLET)
	// Can't use this until we no longer support Qt versions older than 5.8:
	//m_ui->formLayout_2->removeRow(m_ui->windowsink);
	m_ui->formLayout_2->removeWidget(m_ui->windowsink);
	m_ui->windowsink->hide();
#endif
#if !defined(Q_OS_WIN)
	m_ui->formLayout_2->removeWidget(m_ui->relativePenModeHack);
	m_ui->relativePenModeHack->hide();
#endif

	// Editable shortcuts
	m_customShortcuts = new CustomShortcutModel(this);
	auto filteredShortcuts = new QSortFilterProxyModel(this);
	filteredShortcuts->setSourceModel(m_customShortcuts);
	connect(m_ui->shortcutFilter, &QLineEdit::textChanged, filteredShortcuts, &QSortFilterProxyModel::setFilterFixedString);
	filteredShortcuts->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_ui->shortcuts->setModel(filteredShortcuts);
	m_ui->shortcuts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	m_ui->shortcuts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

	// QKeySequence editor delegate
	QStyledItemDelegate *keyseqdel = new QStyledItemDelegate(this);
	QItemEditorFactory *itemeditorfactory = new QItemEditorFactory;
	itemeditorfactory->registerEditor(compat::metaTypeFromName("QKeySequence"), new KeySequenceEditFactory);
	keyseqdel->setItemEditorFactory(itemeditorfactory);
	m_ui->shortcuts->setItemDelegateForColumn(1, keyseqdel);
	m_ui->shortcuts->setItemDelegateForColumn(2, keyseqdel);

	// Deselect item before saving. This causes the editor widget to close
	// and commit the change.
	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, [this]() {
		m_ui->shortcuts->setCurrentIndex(QModelIndex());
	});

	// Known hosts list
	connect(m_ui->knownHostList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(viewCertificate(QListWidgetItem*)));
	connect(m_ui->knownHostList, SIGNAL(itemSelectionChanged()), this, SLOT(certificateSelectionChanged()));
	connect(m_ui->trustKnownHosts, SIGNAL(clicked()), this, SLOT(markTrustedCertificates()));
	connect(m_ui->removeKnownHosts, SIGNAL(clicked()), this, SLOT(removeCertificates()));
	connect(m_ui->importTrustedButton, SIGNAL(clicked()), this, SLOT(importTrustedCertificate()));

	QStringList pemfilter; pemfilter << "*.pem";
	QDir knownHostsDir(utils::paths::writablePath("known-hosts"));

	for(const QString &filename : knownHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(filename.left(filename.length()-4), m_ui->knownHostList);
		i->setData(Qt::UserRole, false);
		i->setData(Qt::UserRole+1, knownHostsDir.absoluteFilePath(filename));
	}

	const QDir trustedHostsDir(utils::paths::writablePath("trusted-hosts"));
	const QIcon trustedIcon = icon::fromTheme("security-high");
	for(const QString &filename : trustedHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(trustedIcon, filename.left(filename.length()-4), m_ui->knownHostList);
		i->setData(Qt::UserRole, true);
		i->setData(Qt::UserRole+1, trustedHostsDir.absoluteFilePath(filename));
	}

	// Session listing server list
	m_listservers = new sessionlisting::ListServerModel(true, this);
	m_ui->listserverview->setModel(m_listservers);
	m_ui->listserverview->setItemDelegate(new sessionlisting::ListServerDelegate(this));

	connect(m_ui->addListServer, &QPushButton::clicked, this, &SettingsDialog::addListingServer);
	connect(m_ui->removeListServer, &QPushButton::clicked, this, &SettingsDialog::removeListingServer);
	connect(m_ui->listserverUp, &QPushButton::clicked, this, &SettingsDialog::moveListingServerUp);
	connect(m_ui->listserverDown, &QPushButton::clicked, this, &SettingsDialog::moveListingServerDown);

	// Parental controls
	connect(m_ui->nsfmLock, &QPushButton::clicked, this, &SettingsDialog::lockParentalControls);

	// Avatar list
	m_avatars = new AvatarListModel(this);
	m_ui->avatarList->setModel(m_avatars);

	connect(m_ui->addAvatar, &QPushButton::clicked, this, &SettingsDialog::addAvatar);
	connect(m_ui->deleteAvatar, &QPushButton::clicked, this, &SettingsDialog::removeSelectedAvatar);

	// Color wheel
	connect(m_ui->colorwheelShapeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &SettingsDialog::changeColorWheelShape);
	connect(m_ui->colorwheelAngleBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &SettingsDialog::changeColorWheelAngle);
	connect(m_ui->colorwheelSpaceBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &SettingsDialog::changeColorWheelSpace);

	// Load configuration
	restoreSettings();

	// Settings saving
	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::rememberSettings);
	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveCertTrustChanges);
	connect(m_ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &SettingsDialog::resetSettings);
	connect(m_ui->buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(resetSettings()));

	// Active first page
	m_ui->pager->setCurrentRow(0);
}

SettingsDialog::~SettingsDialog()
{
	delete m_ui;
}

void SettingsDialog::resetSettings()
{
	QMessageBox::StandardButton b = QMessageBox::question(
				this,
				tr("Reset settings"),
				tr("Clear all settings?")
				);
	if(b==QMessageBox::Yes) {
		QSettings cfg;
		const QVariant pclevel = cfg.value("pc/level");
		const QVariant pclocked = cfg.value("pc/locked");
		cfg.clear();

		// Do not reset parental controls if locked
		if(!pclocked.toByteArray().isEmpty() || parentalcontrols::isOSActive()) {
			cfg.setValue("pc/level", pclevel);
		}

		restoreSettings();
		rememberSettings();
	}
}

void SettingsDialog::restoreSettings()
{
	QSettings cfg;

	cfg.beginGroup("notifications");
	m_ui->notificationVolume->setValue(cfg.value("volume", 40).toInt());
	m_ui->notifChat->setChecked(cfg.value("chat", true).toBool());
	m_ui->notifMarker->setChecked(cfg.value("marker", true).toBool());
	m_ui->notifLogin->setChecked(cfg.value("login", true).toBool());
	m_ui->notifLock->setChecked(cfg.value("lock", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings");
	{
		QVariant langOverride = cfg.value("language", QString());
		for(int i=1;i<m_ui->languageBox->count();++i) {
			if(m_ui->languageBox->itemData(i) == langOverride) {
				m_ui->languageBox->setCurrentIndex(i);
				break;
			}
		}
	}

#ifndef Q_OS_MACOS
	m_ui->themeChoice->setCurrentIndex(cfg.value("theme", 0).toInt());
#endif
	m_ui->logfile->setChecked(cfg.value("logfile", true).toBool());
	m_ui->autosaveInterval->setValue(cfg.value("autosave", 5000).toInt() / 1000);

	m_ui->brushCursorBox->setCurrentIndex(cfg.value("brushcursor").toInt());
	m_ui->brushOutlineWidth->setValue(cfg.value("brushoutlinewidth", 1.0).toReal());
	m_ui->toolToggleShortcut->setChecked(cfg.value("tooltoggle", true).toBool());
	m_ui->shareBrushSlotColor->setChecked(cfg.value("sharebrushslotcolor", false).toBool());

	m_ui->insecurePasswordStorage->setChecked(cfg.value("insecurepasswordstorage", false).toBool());

	cfg.endGroup();

	cfg.beginGroup("versioncheck");
	m_ui->checkForUpdates->setChecked(cfg.value("enabled", true).toBool());
	m_ui->checkForBetas->setEnabled(cfg.value("enabled", true).toBool());
	m_ui->checkForBetas->setChecked(cfg.value("beta", false).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings/input");
#if defined(Q_OS_WIN) && defined(KIS_TABLET)
	m_ui->windowsink->setChecked(cfg.value("windowsink", true).toBool());
	m_ui->relativePenModeHack->setChecked(cfg.value("relativepenhack", false).toBool());
#endif
	m_ui->viewportEntryHack->setChecked(cfg.value("viewportentryhack").toBool());
	m_ui->tabletSupport->setChecked(cfg.value("tabletevents", true).toBool());
	m_ui->tabletEraser->setChecked(cfg.value("tableteraser", true).toBool());
#ifdef Q_OS_MACOS
	// Gesture scrolling is always enabled on Macs
	m_ui->touchscroll->setChecked(true);
	m_ui->touchscroll->setEnabled(false);
#else
	m_ui->touchscroll->setChecked(cfg.value("touchscroll", true).toBool());
#endif
	m_ui->touchpinch->setChecked(cfg.value("touchpinch", true).toBool());
	m_ui->touchtwist->setChecked(cfg.value("touchtwist", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings/recording");
	m_ui->recordpause->setChecked(cfg.value("recordpause", true).toBool());
	m_ui->minimumpause->setValue(cfg.value("minimumpause", 0.5).toFloat());
	m_ui->recordtimestamp->setChecked(cfg.value("recordtimestamp", false).toBool());
	m_ui->timestampInterval->setValue(cfg.value("timestampinterval", 15).toInt());
	cfg.endGroup();

	cfg.beginGroup("settings/animation");
	m_ui->onionskinsBelow->setValue(cfg.value("onionskinsbelow", 4).toInt());
	m_ui->onionskinsAbove->setValue(cfg.value("onionskinsabove", 4).toInt());
	m_ui->onionskinTint->setChecked(cfg.value("onionskintint", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings/server");
	m_ui->serverport->setValue(cfg.value("port",DRAWPILE_PROTO_DEFAULT_PORT).toInt());
	m_ui->lowspaceAutoreset->setChecked(cfg.value("autoreset", true).toBool());
	m_ui->connTimeout->setValue(cfg.value("timeout", 60).toInt());
#ifdef HAVE_DNSSD
	m_ui->dnssd->setChecked(cfg.value("dnssd", true).toBool());
#else
	m_ui->dnssd->setEnabled(false);
#endif
	m_ui->useupnp->setEnabled(false);
	m_ui->privateUserList->setChecked(cfg.value("privateUserList", false).toBool());
	cfg.endGroup();

	cfg.beginGroup("pc");
	switch(parentalcontrols::level()) {
	case parentalcontrols::Level::Unrestricted: m_ui->nsfmUnrestricted->setChecked(true); break;
	case parentalcontrols::Level::NoList: m_ui->nsfmHide->setChecked(true); break;
	case parentalcontrols::Level::NoJoin: m_ui->nsfmNoJoin->setChecked(true); break;
	case parentalcontrols::Level::Restricted: m_ui->nsfmDisconnect->setChecked(true); break;
	}
	m_ui->nsfmWords->setPlainText(cfg.value("tagwords", parentalcontrols::defaultWordList()).toString());
	m_ui->autotagNsfm->setChecked(cfg.value("autotag", true).toBool());
	m_ui->noUncensoring->setChecked(cfg.value("noUncensoring", false).toBool());
	setParentalControlsLocked(parentalcontrols::isLocked());
	if(parentalcontrols::isOSActive())
		m_ui->nsfmLock->setEnabled(false);
	cfg.endGroup();

	cfg.beginGroup("settings/canvasShortcuts");
	const auto viewShortcuts = CanvasViewShortcuts::load(cfg);
	m_ui->colorPickKeys->setModifiers(viewShortcuts.colorPick);
	m_ui->layerPickKeys->setModifiers(viewShortcuts.layerPick);
	m_ui->dragRotateKeys->setModifiers(viewShortcuts.dragRotate);
	m_ui->dragZoomKeys->setModifiers(viewShortcuts.dragZoom);
	m_ui->dragQuickAdjustKeys->setModifiers(viewShortcuts.dragQuickAdjust);
	m_ui->scrollRotateKeys->setModifiers(viewShortcuts.scrollRotate);
	m_ui->scrollZoomKeys->setModifiers(viewShortcuts.scrollZoom);
	m_ui->scrollQuickAdjustKeys->setModifiers(viewShortcuts.scrollQuickAdjust);
	m_ui->toolConstrain1Keys->setModifiers(viewShortcuts.toolConstraint1);
	m_ui->toolConstrain2Keys->setModifiers(viewShortcuts.toolConstraint2);
	cfg.endGroup();

	cfg.beginGroup("settings/colorwheel");
	changeColorWheelShape(cfg.value("shape").toInt());
	changeColorWheelAngle(cfg.value("rotate").toInt());
	changeColorWheelSpace(cfg.value("space").toInt());
	cfg.endGroup();

	cfg.beginGroup("settings/brushsliderlimits");
	m_ui->sizeLimitSpinner->setValue(cfg.value(
			"size", tools::BrushSettings::DEFAULT_BRUSH_SIZE).toInt());
	m_ui->spacingLimitSpinner->setValue(cfg.value(
			"spacing", tools::BrushSettings::DEFAULT_BRUSH_SPACING).toInt());
	cfg.endGroup();

	m_customShortcuts->loadShortcuts();
	m_avatars->loadAvatars();
}

void SettingsDialog::setParentalControlsLocked(bool lock)
{
	m_ui->nsfmUnrestricted->setDisabled(lock);
	m_ui->nsfmHide->setDisabled(lock);
	m_ui->nsfmNoJoin->setDisabled(lock);
	m_ui->nsfmDisconnect->setDisabled(lock);
	m_ui->noUncensoring->setDisabled(lock);
	m_ui->nsfmLock->setText(lock ? tr("Unlock") : tr("Lock"));
}

void SettingsDialog::rememberSettings()
{
	QSettings cfg;
	// Remember notification settings
	cfg.beginGroup("notifications");
	cfg.setValue("volume", m_ui->notificationVolume->value());
	cfg.setValue("chat", m_ui->notifChat->isChecked());
	cfg.setValue("marker", m_ui->notifMarker->isChecked());
	cfg.setValue("login", m_ui->notifLogin->isChecked());
	cfg.setValue("lock", m_ui->notifLock->isChecked());
	cfg.endGroup();

	// Remember general settings
	cfg.setValue("settings/language", m_ui->languageBox->currentData());
#ifndef Q_OS_MACOS
	cfg.setValue("settings/theme", m_ui->themeChoice->currentIndex());
#endif
	cfg.setValue("settings/logfile", m_ui->logfile->isChecked());
	cfg.setValue("settings/autosave", m_ui->autosaveInterval->value() * 1000);
	cfg.setValue("settings/brushcursor", m_ui->brushCursorBox->currentIndex());
	cfg.setValue("settings/brushoutlinewidth", static_cast<qreal>(m_ui->brushOutlineWidth->value()));
	cfg.setValue("settings/tooltoggle", m_ui->toolToggleShortcut->isChecked());
	cfg.setValue("settings/sharebrushslotcolor", m_ui->shareBrushSlotColor->isChecked());
	cfg.setValue("settings/insecurepasswordstorage", m_ui->insecurePasswordStorage->isChecked());
	cfg.setValue("versioncheck/enabled", m_ui->checkForUpdates->isChecked());
	cfg.setValue("versioncheck/beta", m_ui->checkForBetas->isChecked());

	cfg.beginGroup("settings/input");
#if defined(Q_OS_WIN) && defined(KIS_TABLET)
	cfg.setValue("windowsink", m_ui->windowsink->isChecked());
	cfg.setValue("relativepenhack", m_ui->relativePenModeHack->isChecked());
#endif
	cfg.setValue("viewportentryhack", m_ui->viewportEntryHack->isChecked());
	cfg.setValue("tabletevents", m_ui->tabletSupport->isChecked());
	cfg.setValue("tableteraser", m_ui->tabletEraser->isChecked());
	cfg.setValue("touchscroll", m_ui->touchscroll->isChecked());
	cfg.setValue("touchpinch", m_ui->touchpinch->isChecked());
	cfg.setValue("touchtwist", m_ui->touchtwist->isChecked());
	cfg.endGroup();

	cfg.beginGroup("settings/recording");
	cfg.setValue("recordpause", m_ui->recordpause->isChecked());
	cfg.setValue("minimumpause", m_ui->minimumpause->value());
	cfg.setValue("recordtimestamp", m_ui->recordtimestamp->isChecked());
	cfg.setValue("timestampinterval", m_ui->timestampInterval->value());
	cfg.endGroup();

	cfg.beginGroup("settings/animation");
	cfg.setValue("onionskinsbelow", m_ui->onionskinsBelow->value());
	cfg.setValue("onionskinsabove", m_ui->onionskinsAbove->value());
	cfg.setValue("onionskintint", m_ui->onionskinTint->isChecked());
	cfg.endGroup();

	// Remember server settings
	cfg.beginGroup("settings/server");
	if(m_ui->serverport->value() == DRAWPILE_PROTO_DEFAULT_PORT)
		cfg.remove("port");
	else
		cfg.setValue("port", m_ui->serverport->value());

	cfg.setValue("autoreset", m_ui->lowspaceAutoreset->isChecked());
	cfg.setValue("timeout", m_ui->connTimeout->value());
	cfg.setValue("dnssd", m_ui->dnssd->isChecked());
	cfg.setValue("upnp", m_ui->useupnp->isChecked());
	cfg.setValue("privateUserList", m_ui->privateUserList->isChecked());

	cfg.endGroup();

	// Remember parental control settings
	cfg.beginGroup("pc");
	cfg.setValue("autotag", m_ui->autotagNsfm->isChecked());
	cfg.setValue("tagwords", m_ui->nsfmWords->toPlainText());
	cfg.setValue("noUncensoring", m_ui->noUncensoring->isChecked());
	cfg.endGroup();

	cfg.beginGroup("settings/canvasShortcuts");
	CanvasViewShortcuts viewShortcuts;
	viewShortcuts.colorPick = m_ui->colorPickKeys->modifiers();
	viewShortcuts.layerPick = m_ui->layerPickKeys->modifiers();
	viewShortcuts.dragRotate = m_ui->dragRotateKeys->modifiers();
	viewShortcuts.dragZoom = m_ui->dragZoomKeys->modifiers();
	viewShortcuts.dragQuickAdjust = m_ui->dragQuickAdjustKeys->modifiers();
	viewShortcuts.scrollRotate = m_ui->scrollRotateKeys->modifiers();
	viewShortcuts.scrollZoom = m_ui->scrollZoomKeys->modifiers();
	viewShortcuts.scrollQuickAdjust = m_ui->scrollQuickAdjustKeys->modifiers();
	viewShortcuts.toolConstraint1 = m_ui->toolConstrain1Keys->modifiers();
	viewShortcuts.toolConstraint2 = m_ui->toolConstrain2Keys->modifiers();
	viewShortcuts.save(cfg);
	cfg.endGroup();

	cfg.beginGroup("settings/colorwheel");
	cfg.setValue("shape", static_cast<int>(m_ui->colorwheel->selectorShape()));
	cfg.setValue("rotate", static_cast<int>(m_ui->colorwheel->rotatingSelector()));
	cfg.setValue("space", static_cast<int>(m_ui->colorwheel->colorSpace()));
	cfg.endGroup();

	cfg.beginGroup("settings/brushsliderlimits");
	cfg.setValue("size", m_ui->sizeLimitSpinner->value());
	cfg.setValue("spacing", m_ui->spacingLimitSpinner->value());
	cfg.endGroup();

	if(!parentalcontrols::isLocked())
		rememberPcLevel();

	m_customShortcuts->saveShortcuts();
	m_listservers->saveServers();
	m_avatars->commit();

	static_cast<DrawpileApp*>(qApp)->notifySettingsChanged();
}

void SettingsDialog::rememberPcLevel()
{
	parentalcontrols::Level level = parentalcontrols::Level::Unrestricted;
	if(m_ui->nsfmHide->isChecked())
		level = parentalcontrols::Level::NoList;
	else if(m_ui->nsfmNoJoin->isChecked())
		level = parentalcontrols::Level::NoJoin;
	else if(m_ui->nsfmDisconnect->isChecked())
		level = parentalcontrols::Level::Restricted;
	QSettings().setValue("pc/level", int(level));
}

void SettingsDialog::saveCertTrustChanges()
{
	// Delete removed certificates
	for(const QString &certfile : qAsConst(m_removeCerts)) {
		QFile(certfile).remove();
	}

	// Move selected certs to trusted certs
	const auto trustedDir = utils::paths::writablePath("trusted-hosts/", ".");

	for(const QString &certfile : qAsConst(m_trustCerts)) {
		QString certname = certfile.mid(certfile.lastIndexOf('/')+1);
		QFile{certfile}.rename(trustedDir + certname);
	}

	// Save imported certificates
	for(const QSslCertificate &cert : qAsConst(m_importCerts)) {
		QString hostname = cert.subjectInfo(QSslCertificate::CommonName).at(0);

		QFile f{trustedDir + hostname + ".pem"};
		if(!f.open(QFile::WriteOnly)) {
			qWarning() << "error opening" << f.fileName() << f.errorString();
			continue;
		}

		f.write(cert.toPem());
	}
}

void SettingsDialog::viewCertificate(QListWidgetItem *item)
{
	QString filename;
	if(item->data(Qt::UserRole+2).isNull())
		filename = item->data(Qt::UserRole+1).toString();
	else // read imported cert from original file
		filename = item->data(Qt::UserRole+2).toString();

	QList<QSslCertificate> certs = QSslCertificate::fromPath(filename);
	if(certs.isEmpty()) {
		qWarning() << "Certificate" << filename << "not found!";
		return;
	}

	CertificateView *cv = new CertificateView(item->text(), certs.at(0), this);
	cv->setAttribute(Qt::WA_DeleteOnClose);
	cv->show();
}

void SettingsDialog::certificateSelectionChanged()
{
	const QItemSelectionModel *sel = m_ui->knownHostList->selectionModel();
	if(sel->selectedIndexes().isEmpty()) {
		m_ui->trustKnownHosts->setEnabled(false);
		m_ui->removeKnownHosts->setEnabled(false);
	} else {
		bool cantrust = false;
		for(const QModelIndex &idx : sel->selectedIndexes()) {
			if(!idx.data(Qt::UserRole).toBool()) {
				cantrust = true;
				break;
			}
		}
		m_ui->trustKnownHosts->setEnabled(cantrust);
		m_ui->removeKnownHosts->setEnabled(true);
	}
}

void SettingsDialog::markTrustedCertificates()
{
	const QIcon trustedIcon = icon::fromTheme("security-high");
	for(QListWidgetItem *item : m_ui->knownHostList->selectedItems()) {
		if(!item->data(Qt::UserRole).toBool()) {
			m_trustCerts.append(item->data(Qt::UserRole+1).toString());
			item->setIcon(trustedIcon);
			item->setData(Qt::UserRole, true);
		}
	}
	m_ui->trustKnownHosts->setEnabled(false);
}

void SettingsDialog::removeCertificates()
{
	for(QListWidgetItem *item : m_ui->knownHostList->selectedItems()) {
		QString path = item->data(Qt::UserRole+1).toString();
		if(path.isEmpty()) {
			QMutableListIterator<QSslCertificate> i(m_importCerts);
			while(i.hasNext()) {
				if(i.next().subjectInfo(QSslCertificate::CommonName).at(0) == item->text())
					i.remove();
			}
		} else {
			m_trustCerts.removeAll(path);
			m_removeCerts.append(path);
		}

		delete item;
	}
}

void SettingsDialog::importTrustedCertificate()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Import trusted certificate"), QString(),
		tr("Certificates (%1)").arg("*.pem *.crt *.cer") + ";;" +
		QApplication::tr("All files (*)")
	);

	if(path.isEmpty())
		return;

	QList<QSslCertificate> certs = QSslCertificate::fromPath(path);
	if(certs.isEmpty() || certs.at(0).isNull()) {
		QMessageBox::warning(this, tr("Import trusted certificate"), tr("Invalid certificate!"));
		return;
	}

	if(certs.at(0).subjectInfo(QSslCertificate::CommonName).isEmpty()) {
		QMessageBox::warning(this, tr("Import trusted certificate"), tr("Certificate common name not set!"));
		return;
	}

	m_importCerts.append(certs.at(0));

	const QIcon trustedIcon = icon::fromTheme("security-high");
	auto *i = new QListWidgetItem(trustedIcon, certs.at(0).subjectInfo(QSslCertificate::CommonName).at(0), m_ui->knownHostList);
	i->setData(Qt::UserRole, true);
	i->setData(Qt::UserRole+2, path);
}

void SettingsDialog::addListingServer()
{
	QString urlstr = QInputDialog::getText(this, tr("Add public listing server"), "URL");
	if(urlstr.isEmpty())
		return;

	if(!urlstr.startsWith("http"))
		urlstr = "http://" + urlstr;

	const QUrl url(urlstr);
	if(!url.isValid()) {
		QMessageBox::warning(this, tr("Add public listing server"), tr("Invalid URL!"));
		return;
	}

	auto *dlg = new AddServerDialog(this);
	dlg->setListServerModel(m_listservers);
	dlg->query(url);
}

void SettingsDialog::moveListingServerUp()
{
	QModelIndex selection = m_ui->listserverview->selectionModel()->currentIndex();
	if(selection.isValid()) {
		m_listservers->moveRow(QModelIndex(), selection.row(), QModelIndex(), selection.row()-1);
	}
}

void SettingsDialog::moveListingServerDown()
{
	QModelIndex selection = m_ui->listserverview->selectionModel()->currentIndex();
	if(selection.isValid()) {
		m_listservers->moveRow(QModelIndex(), selection.row(), QModelIndex(), selection.row()+1);
	}
}

void SettingsDialog::removeListingServer()
{
	QModelIndex selection = m_ui->listserverview->selectionModel()->currentIndex();
	if(selection.isValid()) {
		m_listservers->removeRow(selection.row());
	}
}

void SettingsDialog::lockParentalControls()
{
	QSettings cfg;
	cfg.beginGroup("pc");

	QByteArray oldpass = cfg.value("locked").toByteArray();
	bool locked = !oldpass.isEmpty();

	QString title, prompt;
	if(locked) {
		title = tr("Unlock Parental Controls");
		prompt = tr("Password");
	} else {
		title = tr("Lock Parental Controls");
		prompt = tr("Set password");
	}

	QString pass = QInputDialog::getText(this, title, prompt, QLineEdit::Password);

	if(!pass.isEmpty()) {
		if(locked) {
			if(server::passwordhash::check(pass, oldpass)) {
				cfg.remove("locked");
				locked = false;
				m_ui->nsfmLock->setText(tr("Lock"));
			} else {
				QMessageBox::warning(this, tr("Unlock Parental Controls"), tr("Incorrect password"));
				return;
			}
		} else {
			cfg.setValue("locked", server::passwordhash::hash(pass));
			locked = true;
			rememberPcLevel();
			m_ui->nsfmLock->setText(tr("Unlock"));
		}
	}

	setParentalControlsLocked(locked);
}

void SettingsDialog::addAvatar()
{
	AvatarImport::importAvatar(m_avatars, this);
}

void SettingsDialog::removeSelectedAvatar()
{
	const QModelIndex idx = m_ui->avatarList->currentIndex();
	if(idx.isValid())
		m_avatars->removeRow(idx.row());
}


void SettingsDialog::changeColorWheelShape(int index)
{
	ColorWheel::ShapeEnum shape;
	switch(index) {
	case ColorWheel::ShapeTriangle:
		shape = ColorWheel::ShapeTriangle;
		break;
	case ColorWheel::ShapeSquare:
		shape = ColorWheel::ShapeSquare;
		break;
	default:
		qWarning() << "Unknown color wheel shape index " << index;
		shape = ColorWheel::ShapeSquare;
		break;
	}
	m_ui->colorwheel->setSelectorShape(shape);
	m_ui->colorwheelShapeBox->setCurrentIndex(shape);
}

void SettingsDialog::changeColorWheelAngle(int index)
{
	ColorWheel::AngleEnum rotating;
	switch(index) {
	case ColorWheel::AngleFixed:
		rotating = ColorWheel::AngleFixed;
		break;
	case ColorWheel::AngleRotating:
		rotating = ColorWheel::AngleRotating;
		break;
	default:
		qWarning() << "Unknown color wheel angle index " << index;
		rotating = ColorWheel::AngleFixed;
		break;
	}
	m_ui->colorwheel->setRotatingSelector(rotating);
	m_ui->colorwheelAngleBox->setCurrentIndex(rotating);
}

void SettingsDialog::changeColorWheelSpace(int index)
{
	ColorWheel::ColorSpaceEnum space;
	switch(index) {
	case ColorWheel::ColorHSV:
		space = ColorWheel::ColorHSV;
		break;
	case ColorWheel::ColorHSL:
		space = ColorWheel::ColorHSL;
		break;
	case ColorWheel::ColorLCH:
		space = ColorWheel::ColorLCH;
		break;
	default:
		qWarning() << "Unknown color wheel space index " << index;
		space = ColorWheel::ColorHSV;
		break;
	}
	m_ui->colorwheel->setColorSpace(space);
	m_ui->colorwheelSpaceBox->setCurrentIndex(space);
}

}

