// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/sessionsettings.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/acl.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/document.h"
#include "libclient/net/banlistmodel.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/utils/listservermodel.h"
#include "libshared/util/qtcompat.h"
#include "ui_sessionsettings.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTemporaryFile>
#include <QTimer>
#include <dpcommon/platform_qt.h>

namespace dialogs {

class SessionSettingsDialog::AuthListImport {
public:
	AuthListImport(const QJsonArray &list)
		: m_count(list.size())
	{
		constexpr compat::sizetype CHUNK_SIZE = 100;
		if(m_count > 0) {
			m_lists.resize(m_count / CHUNK_SIZE + 1);
			for(compat::sizetype i = 0; i < m_count; ++i) {
				m_lists[i / CHUNK_SIZE].append(list[i]);
			}
		}
	}

	bool isEmpty() const { return m_lists.isEmpty(); }
	QJsonArray takeFirst() { return m_lists.takeFirst(); }
	compat::sizetype count() const { return m_count; }
	void markAsDone() { m_done = true; }
	bool isDone() const { return m_done; }

private:
	QVector<QJsonArray> m_lists;
	const compat::sizetype m_count;
	bool m_done = false;
};

SessionSettingsDialog::SessionSettingsDialog(Document *doc, QWidget *parent)
	: QDialog(parent)
	, m_ui(new Ui_SessionSettingsDialog)
	, m_doc(doc)
{
	Q_ASSERT(doc);
	m_ui->setupUi(this);

	initPermissionComboBoxes();
	initPermissionLimitSliders();

	connect(
		m_doc, &Document::canvasChanged, this,
		&SessionSettingsDialog::onCanvasChanged);

	// Set up the settings page
	m_saveTimer = new QTimer(this);
	m_saveTimer->setSingleShot(true);
	m_saveTimer->setInterval(1000);
	connect(
		m_saveTimer, &QTimer::timeout, this,
		&SessionSettingsDialog::sendSessionConf);

	connect(
		m_ui->title, &QLineEdit::textEdited, this,
		&SessionSettingsDialog::titleChanged);
	connect(
		m_ui->maxUsers, &QSpinBox::editingFinished, this,
		&SessionSettingsDialog::maxUsersChanged);
	connect(
		m_ui->denyJoins, &QCheckBox::clicked, this,
		&SessionSettingsDialog::denyJoinsChanged);
	connect(
		m_ui->authOnly, &QCheckBox::clicked, this,
		&SessionSettingsDialog::authOnlyChanged);
	connect(
		m_ui->allowWeb, &QCheckBox::clicked, this,
		&SessionSettingsDialog::allowWebChanged);
	connect(
		m_ui->autoresetThreshold, &QDoubleSpinBox::editingFinished, this,
		&SessionSettingsDialog::autoresetThresholdChanged);
	connect(
		m_ui->preserveChat, &QCheckBox::clicked, this,
		&SessionSettingsDialog::keepChatChanged);
	connect(
		m_ui->persistent, &QCheckBox::clicked, this,
		&SessionSettingsDialog::persistenceChanged);
	connect(
		m_ui->nsfm, &QCheckBox::clicked, this,
		&SessionSettingsDialog::nsfmChanged);
	connect(
		m_ui->deputies, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &SessionSettingsDialog::deputiesChanged);
	connect(
		m_ui->idleTimeoutOverride, &QCheckBox::clicked, this,
		&SessionSettingsDialog::idleOverrideChanged);

	connect(
		m_ui->sessionPassword, &QLabel::linkActivated, this,
		&SessionSettingsDialog::changePassword);
	connect(
		m_ui->opword, &QLabel::linkActivated, this,
		&SessionSettingsDialog::changeOpword);

	connect(
		m_doc, &Document::sessionTitleChanged, m_ui->title,
		&QLineEdit::setText);
	connect(
		m_doc, &Document::sessionPreserveChatChanged, m_ui->preserveChat,
		&QCheckBox::setChecked);
	connect(
		m_doc, &Document::sessionPersistentChanged, m_ui->persistent,
		&QCheckBox::setChecked);
	connect(
		m_doc, &Document::sessionClosedChanged, m_ui->denyJoins,
		&QCheckBox::setChecked);
	connect(
		m_doc, &Document::sessionAuthOnlyChanged, this, [this](bool authOnly) {
			m_ui->authOnly->setEnabled(m_op && (authOnly || m_isAuth));
			m_ui->authOnly->setChecked(authOnly);
		});
	connect(
		m_doc, &Document::sessionAllowWebChanged, this,
		&SessionSettingsDialog::updateAllowWebCheckbox);
	connect(
		m_doc, &Document::sessionHasPasswordChanged, this,
		[this](bool hasPassword) {
			m_ui->sessionPassword->setProperty("haspass", hasPassword);
			updatePasswordLabel(m_ui->sessionPassword);
		});
	connect(
		m_doc, &Document::sessionOpwordChanged, this, [this](bool hasPassword) {
			m_ui->opword->setProperty("haspass", hasPassword);
			updatePasswordLabel(m_ui->opword);
		});
	connect(
		m_doc, &Document::sessionNsfmChanged, this,
		&SessionSettingsDialog::updateNsfmCheckbox);
	connect(
		m_doc, &Document::sessionForceNsfmChanged, this,
		&SessionSettingsDialog::updateNsfmCheckbox);
	connect(
		m_doc, &Document::sessionDeputiesChanged, this, [this](bool deputies) {
			m_ui->deputies->setCurrentIndex(deputies ? 1 : 0);
		});
	connect(
		m_doc, &Document::sessionIdleChanged, this,
		&SessionSettingsDialog::updateIdleSettings);
	connect(
		m_doc, &Document::sessionMaxUserCountChanged, m_ui->maxUsers,
		&QSpinBox::setValue);
	connect(
		m_doc, &Document::sessionResetThresholdChanged,
		m_ui->autoresetThreshold, &QDoubleSpinBox::setValue);
	connect(
		m_doc, &Document::baseResetThresholdChanged, this,
		[this](int threshold) {
			m_ui->baseResetThreshold->setText(QStringLiteral("+ %1 MB").arg(
				threshold / (1024.0 * 1024.0), 0, 'f', 1));
		});

	// Set up permissions tab
	m_ui->permBrushSize->setExponentRatio(3.0);
	connect(
		m_ui->permBrushSize,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&SessionSettingsDialog::updateBrushSizeLimitText);
	connect(
		m_ui->permissionPresets, &widgets::PresetSelector::saveRequested, this,
		&SessionSettingsDialog::permissionPresetSaving);
	connect(
		m_ui->permissionPresets, &widgets::PresetSelector::loadRequested, this,
		&SessionSettingsDialog::permissionPresetSelected);

	// Set up banlist tab
	m_ui->banlistView->setModel(doc->banlist());
	QHeaderView *banlistHeader = m_ui->banlistView->horizontalHeader();
	banlistHeader->setSectionResizeMode(1, QHeaderView::Stretch);
	banlistHeader->setSectionResizeMode(2, QHeaderView::Stretch);
	connect(m_ui->removeBan, &QPushButton::clicked, [this]() {
		const int id = m_ui->banlistView->selectionModel()
						   ->currentIndex()
						   .data(Qt::UserRole)
						   .toInt();
		if(id > 0) {
			qDebug() << "requesting removal of in-session ban entry" << id;
			m_doc->sendUnban(id);
		}
	});
	connect(
		m_ui->importButton, &QAbstractButton::clicked, this,
		&SessionSettingsDialog::importBans);
	connect(
		m_ui->exportButton, &QAbstractButton::clicked, this,
		&SessionSettingsDialog::exportBans);

	// Set up auth list tab
	QSortFilterProxyModel *authListSortModel = new QSortFilterProxyModel(this);
	net::AuthListModel *authList = doc->authList();
	authListSortModel->setSourceModel(authList);
	authListSortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_ui->authListView->setModel(authListSortModel);

	QHeaderView *authListHeader = m_ui->authListView->horizontalHeader();
	authListHeader->setSectionResizeMode(0, QHeaderView::Stretch);
	authListHeader->setSectionResizeMode(1, QHeaderView::Stretch);

	// The viewport only repaints when it has focus by default, force it.
	connect(
		authList, &net::AuthListModel::updateApplied,
		m_ui->authListView->viewport(), QOverload<>::of(&QWidget::update));
	connect(
		authList, &net::AuthListModel::updateApplied, this,
		&SessionSettingsDialog::updateAuthListCheckboxes);
	connect(
		m_ui->authListView->selectionModel(),
		&QItemSelectionModel::selectionChanged, this,
		&SessionSettingsDialog::updateAuthListCheckboxes);
	connect(
		m_ui->authImportButton, &QAbstractButton::clicked, this,
		&SessionSettingsDialog::importAuthList);
	connect(
		m_ui->authExportButton, &QAbstractButton::clicked, this,
		&SessionSettingsDialog::exportAuthList);
	connect(
		m_ui->authOp, &QCheckBox::clicked, this,
		&SessionSettingsDialog::authListChangeOp);
	connect(
		m_ui->authTrusted, &QCheckBox::clicked, this,
		&SessionSettingsDialog::authListChangeTrusted);

	m_ui->announcementListView->setModel(doc->announcementList());

	QMenu *addAnnouncementMenu = new QMenu(this);

	m_ui->addAnnouncement->setMenu(addAnnouncementMenu);

	connect(addAnnouncementMenu, &QMenu::triggered, [this](QAction *a) {
		const QString apiUrl = a->property("API_URL").toString();
		qDebug() << "Requesting public announcement:" << apiUrl;
		m_doc->sendAnnounce(apiUrl);
	});

	connect(m_ui->removeAnnouncement, &QPushButton::clicked, [this]() {
		QItemSelection sel =
			m_ui->announcementListView->selectionModel()->selection();
		QString apiUrl;
		if(!sel.isEmpty())
			apiUrl =
				sel.first().indexes().first().data(Qt::UserRole).toString();
		if(!apiUrl.isEmpty()) {
			qDebug() << "Requesting unlisting:" << apiUrl;
			m_doc->sendUnannounce(apiUrl);
		}
	});

	// Metadata permissions are currently only useful for changing image DPI,
	// which isn't even possible from within Drawpile and generally useless.
	// So we'll hide that setting for now, until it actually does something.
	m_ui->permMetadata->hide();
	m_ui->labelMetadata->hide();
	updateBanImportExportState();
	m_ui->allowWeb->setEnabled(false);
	m_ui->allowWeb->setVisible(false);
	updateIdleSettings(0, false, false);
}

SessionSettingsDialog::~SessionSettingsDialog()
{
	delete m_ui;
}

void SessionSettingsDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	reloadSettings();
}

void SessionSettingsDialog::closeEvent(QCloseEvent *event)
{
	if(!isEnabled()) {
		event->ignore();
	}
}

void SessionSettingsDialog::reloadSettings()
{
	const auto listservers = sessionlisting::ListServerModel::listServers(
		dpApp().settings().listServers(), false);
	auto *addAnnouncementMenu = m_ui->addAnnouncement->menu();

	addAnnouncementMenu->clear();

	for(const auto &listserver : listservers) {
		if(listserver.publicListings) {
			QAction *a = addAnnouncementMenu->addAction(
				listserver.icon, listserver.name);
			a->setProperty("API_URL", listserver.url);
		}
	}

	m_ui->addAnnouncement->setEnabled(!addAnnouncementMenu->isEmpty());
}

void SessionSettingsDialog::setPersistenceEnabled(bool enable)
{
	m_ui->persistent->setEnabled(m_op && enable);
	m_canPersist = enable;
}

void SessionSettingsDialog::setBanImpExEnabled(
	bool isModerator, bool cryptEnabled, bool modEnabled)
{
	m_canCryptImpExBans = cryptEnabled;
	m_canModImportBans = modEnabled;
	m_canModExportBans = isModerator && modEnabled;
	m_awaitingImportExportResponse = false;
	updateBanImportExportState();
}

void SessionSettingsDialog::setAutoResetEnabled(bool enable)
{
	m_ui->autoresetThreshold->setEnabled(m_op && enable);
	m_canAutoreset = enable;
}

void SessionSettingsDialog::setAuthenticated(bool auth)
{
	m_isAuth = auth;
	// auth-only can only be enabled if the current user is authenticated,
	// otherwise it's possible to accidentally lock yourself out.
	m_ui->authOnly->setEnabled(
		m_op && (m_isAuth || m_ui->authOnly->isChecked()));
}

void SessionSettingsDialog::bansImported(int total, int imported)
{
	if(!m_awaitingImportExportResponse) {
		return;
	}
	m_awaitingImportExportResponse = false;
	updateBanImportExportState();

	QString message = tr("Imported %n session ban(s).", "", imported);
	if(imported < total) {
		//: %1 is the "Imported %n session ban(s)." message.
		message = tr("%1 %n was/were not imported because it was/they were "
					 "invalid or duplicates.",
					 "", total - imported)
					  .arg(message);
	}
	utils::showInformation(this, tr("Session Ban Import"), message);
}

void SessionSettingsDialog::bansExported(const QByteArray &bans)
{
	if(!m_awaitingImportExportResponse) {
		return;
	}
	m_awaitingImportExportResponse = false;
	updateBanImportExportState();

	QString error;
	if(!FileWrangler(this).saveSessionBans(bans, &error)) {
		utils::showCritical(
			this, tr("Session Ban Export"),
			tr("Error saving bans: %1").arg(error));
	}
}

void SessionSettingsDialog::bansImpExError(const QString &message)
{
	if(!m_awaitingImportExportResponse) {
		return;
	}
	m_awaitingImportExportResponse = false;
	updateBanImportExportState();

	utils::showCritical(this, tr("Session Ban Error"), message);
}

void SessionSettingsDialog::onCanvasChanged(canvas::CanvasModel *canvas)
{
	if(!canvas)
		return;

	canvas::AclState *acl = canvas->aclState();

	connect(
		acl, &canvas::AclState::localOpChanged, this,
		&SessionSettingsDialog::onOperatorModeChanged);
	connect(
		acl, &canvas::AclState::featureTiersChanged, this,
		&SessionSettingsDialog::onFeatureTiersChanged);

	onOperatorModeChanged(acl->amOperator());
	onFeatureTiersChanged(acl->featureTiers());
}

void SessionSettingsDialog::onOperatorModeChanged(bool op)
{
	m_op = op;

	QWidget *widgets[] = {
		m_ui->title,		   m_ui->maxUsers,		  m_ui->denyJoins,
		m_ui->preserveChat,	   m_ui->deputies,		  m_ui->sessionPassword,
		m_ui->opword,		   m_ui->addAnnouncement, m_ui->removeAnnouncement,
		m_ui->removeBan,	   m_ui->authListView,	  m_ui->authImportButton,
		m_ui->authExportButton};
	for(QWidget *widget : widgets) {
		widget->setEnabled(op);
	}

	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		featureBox(DP_Feature(i))->setEnabled(op);
	}

	m_ui->persistent->setEnabled(m_canPersist && op);
	m_ui->autoresetThreshold->setEnabled(m_canAutoreset && op);
	m_ui->nsfm->setEnabled(!m_doc->isSessionForceNsfm() && op);
	m_ui->authOnly->setEnabled(op && (m_isAuth || m_ui->authOnly->isChecked()));
	m_ui->allowWeb->setEnabled(op && m_canAlterAllowWeb);
	m_ui->permissionPresets->setWriteOnly(!op);
	updatePasswordLabel(m_ui->sessionPassword);
	updatePasswordLabel(m_ui->opword);

	m_ui->authLabel->setText(
		op ? tr("This list shows only registered users.")
		   : tr("Only operators can see this list."));
	updateAuthListCheckboxes();
	updateBanImportExportState();
}

QComboBox *SessionSettingsDialog::featureBox(DP_Feature f)
{
	switch(f) {
	case DP_FEATURE_PUT_IMAGE:
		return m_ui->permPutImage;
	case DP_FEATURE_REGION_MOVE:
		return m_ui->permRegionMove;
	case DP_FEATURE_RESIZE:
		return m_ui->permResize;
	case DP_FEATURE_BACKGROUND:
		return m_ui->permBackground;
	case DP_FEATURE_EDIT_LAYERS:
		return m_ui->permEditLayers;
	case DP_FEATURE_OWN_LAYERS:
		return m_ui->permOwnLayers;
	case DP_FEATURE_CREATE_ANNOTATION:
		return m_ui->permCreateAnnotation;
	case DP_FEATURE_LASER:
		return m_ui->permLaser;
	case DP_FEATURE_UNDO:
		return m_ui->permUndo;
	case DP_FEATURE_METADATA:
		return m_ui->permMetadata;
	case DP_FEATURE_TIMELINE:
		return m_ui->permTimeline;
	case DP_FEATURE_MYPAINT:
		return m_ui->permMyPaint;
	default:
		Q_ASSERT_X(false, "featureBox", "unhandled case");
		return nullptr;
	}
}

KisSliderSpinBox *SessionSettingsDialog::limitSlider(DP_FeatureLimit fl)
{
	switch(fl) {
	case DP_FEATURE_LIMIT_BRUSH_SIZE:
		return m_ui->permBrushSize;
	default:
		Q_ASSERT_X(false, "limitSlider", "unhandled case");
		return nullptr;
	}
}

int SessionSettingsDialog::limitSliderValue(DP_FeatureLimit fl)
{
	switch(fl) {
	case DP_FEATURE_LIMIT_BRUSH_SIZE: {
		int value = m_ui->permBrushSize->value();
		return value < m_ui->permBrushSize->maximum() ? value : -1;
	}
	default:
		Q_ASSERT_X(false, "limitSliderValue", "unhandled case");
		return -1;
	}
}

void SessionSettingsDialog::setLimitSliderValue(DP_FeatureLimit fl, int value)
{
	KisSliderSpinBox *slider = limitSlider(fl);
	QSignalBlocker blocker(slider);
	slider->setValue(value < 0 ? slider->maximum() : value);
}

void SessionSettingsDialog::onFeatureTiersChanged(
	const DP_FeatureTiers &features)
{
	m_ui->permPutImage->setCurrentIndex(
		int(features.tiers[DP_FEATURE_PUT_IMAGE]));
	m_ui->permRegionMove->setCurrentIndex(
		int(features.tiers[DP_FEATURE_REGION_MOVE]));
	m_ui->permResize->setCurrentIndex(int(features.tiers[DP_FEATURE_RESIZE]));
	m_ui->permBackground->setCurrentIndex(
		int(features.tiers[DP_FEATURE_BACKGROUND]));
	m_ui->permEditLayers->setCurrentIndex(
		int(features.tiers[DP_FEATURE_EDIT_LAYERS]));
	m_ui->permOwnLayers->setCurrentIndex(
		int(features.tiers[DP_FEATURE_OWN_LAYERS]));
	m_ui->permCreateAnnotation->setCurrentIndex(
		int(features.tiers[DP_FEATURE_CREATE_ANNOTATION]));
	m_ui->permLaser->setCurrentIndex(int(features.tiers[DP_FEATURE_LASER]));
	m_ui->permUndo->setCurrentIndex(int(features.tiers[DP_FEATURE_UNDO]));
	m_ui->permMetadata->setCurrentIndex(
		int(features.tiers[DP_FEATURE_METADATA]));
	m_ui->permTimeline->setCurrentIndex(
		int(features.tiers[DP_FEATURE_TIMELINE]));
	m_ui->permMyPaint->setCurrentIndex(int(features.tiers[DP_FEATURE_MYPAINT]));
	setLimitSliderValue(
		DP_FEATURE_LIMIT_BRUSH_SIZE,
		features.limits[DP_FEATURE_LIMIT_BRUSH_SIZE][DP_ACCESS_TIER_GUEST]);
}

const QByteArray SessionSettingsDialog::authExportPrefix =
	QByteArray("dpauth\0", 7);

void SessionSettingsDialog::initPermissionComboBoxes()
{
	// Note: these must match the canvas::Tier enum
	const QString items[] = {
		tr("Operators"), tr("Trusted"), tr("Registered"), tr("Everyone")};

	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		QComboBox *box = featureBox(DP_Feature(i));
		for(uint j = 0; j < sizeof(items) / sizeof(QString); ++j)
			box->addItem(items[j]);

		box->setProperty("featureIdx", i);
		connect(
			box, QOverload<int>::of(&QComboBox::activated), this,
			&SessionSettingsDialog::permissionChanged);
	}
}

void SessionSettingsDialog::initPermissionLimitSliders()
{
	for(int i = 0; i < DP_FEATURE_LIMIT_COUNT; ++i) {
		KisSliderSpinBox *slider = limitSlider(DP_FeatureLimit(i));
		connect(
			slider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
			&SessionSettingsDialog::limitChanged);
	}
}

void SessionSettingsDialog::permissionChanged()
{
	m_featureTiersChanged = true;
	m_saveTimer->start();
}

void SessionSettingsDialog::limitChanged()
{
	m_featureLimitsChanged = true;
	m_saveTimer->start();
}

void SessionSettingsDialog::updateBrushSizeLimitText(int value)
{
	m_ui->permBrushSize->setOverrideText(
		value < m_ui->permBrushSize->maximum() ? QString() : tr("Unlimited"));
}

void SessionSettingsDialog::permissionPresetSelected(const QString &presetFile)
{
	QFile f(presetFile);
	if(!f.open(QFile::ReadOnly)) {
		qWarning("%s: could not open file", qPrintable(presetFile));
		return;
	}

	QJsonObject cfg = QJsonDocument::fromJson(f.readAll()).object();

	// Normal features
	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		auto *box = featureBox(DP_Feature(i));
		box->setCurrentIndex(
			cfg.value(box->objectName()).toInt(box->currentIndex()));
	}
	permissionChanged();

	// Deputies
	{
		auto *box = m_ui->deputies;
		box->setCurrentIndex(
			cfg.value(box->objectName()).toInt(box->currentIndex()));
		deputiesChanged(box->currentIndex());
	}
}

void SessionSettingsDialog::permissionPresetSaving(const QString &presetFile)
{
	QJsonObject cfg;

	// Normal features
	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		auto *box = featureBox(DP_Feature(i));
		cfg[box->objectName()] = box->currentIndex();
	}

	// Deputies
	{
		auto *box = m_ui->deputies;
		cfg[box->objectName()] = box->currentIndex();
	}

	// Save
	QFile f(presetFile);
	if(!f.open(DP_QT_WRITE_FLAGS)) {
		qWarning("%s: could not open file", qPrintable(presetFile));
		return;
	}
	f.write(QJsonDocument(cfg).toJson());
}

void SessionSettingsDialog::updatePasswordLabel(QLabel *label)
{
	bool hasPass = label->property("haspass").toBool();
	QString txt =
		QStringLiteral("<b>%1</b>")
			.arg(hasPass ? tr("yes", "password") : tr("no", "password"));

	if(m_op) {
		txt.append(QStringLiteral(" (<a href=\"#\">%1</a>)")
					   .arg(
						   hasPass ? tr("change", "password")
								   : tr("assign", "password")));
	}

	label->setText(txt);
}

void SessionSettingsDialog::updateAllowWebCheckbox(bool allowWeb, bool canAlter)
{
	QSignalBlocker blocker(m_ui->allowWeb);
	m_canAlterAllowWeb = canAlter;
	m_ui->allowWeb->setChecked(allowWeb);
	m_ui->allowWeb->setEnabled(m_op && canAlter);
}

void SessionSettingsDialog::updateNsfmCheckbox(bool)
{
	if(m_doc->isSessionForceNsfm()) {
		m_ui->nsfm->setEnabled(false);
		m_ui->nsfm->setChecked(true);
	} else {
		m_ui->nsfm->setEnabled(m_op);
		m_ui->nsfm->setChecked(m_doc->isSessionNsfm());
	}
}

void SessionSettingsDialog::updateIdleSettings(
	int timeLimit, bool overridden, bool canOverride)
{
	QSignalBlocker blocker(m_ui->idleTimeoutOverride);
	m_ui->idleTimeoutOverride->setChecked(overridden);
	m_ui->idleTimeoutOverride->setEnabled(canOverride);
	m_ui->idleTimeoutOverride->setVisible(canOverride);

	bool visible = timeLimit > 0 || canOverride;
	m_ui->idleTimeoutLabel->setVisible(visible);
	m_ui->idleTimeoutAmount->setVisible(visible);

	QString amount;
	if(timeLimit <= 0) {
		//: "Idle timeout: never"
		amount = tr("never");
	} else if(overridden && !canOverride) {
		//: "Idle timeout: disabled by moderator"
		amount = tr("disabled by moderator");
	} else {
		QStringList pieces;
		int hours = timeLimit / 3600;
		if(hours != 0) {
			//: Idle timeout hours. May be joined with minutes and seconds.
			pieces.append(tr("%n hour(s)", "", hours));
		}

		int minutes = timeLimit / 60 - hours * 60;
		if(minutes != 0) {
			//: Idle timeout minutes. May be joined with hours and seconds.
			pieces.append(tr("%n minute(s)", "", minutes));
		}

		int seconds = timeLimit % 60;
		if(seconds != 0) {
			//: Idle timeout seconds. May be joined with hours and minutes.
			pieces.append(tr("%n second(s)", "", seconds));
		}

		//: This string joins the hours, minutes and seconds for the idle time.
		amount = pieces.join(tr(", "));
	}
	m_ui->idleTimeoutAmount->setText(amount);
}

void SessionSettingsDialog::updateAuthListCheckboxes()
{
	bool enableOp = false;
	bool enableTrusted = false;
	bool checkOp = false;
	bool checkTrusted = false;
	QModelIndex idx = getSelectedAuthListEntry();
	if(idx.isValid() && !idx.data(net::AuthListModel::IsModRole).toBool()) {
		enableOp = !idx.data(net::AuthListModel::IsOwnRole).toBool();
		enableTrusted = true;
		checkOp = idx.data(net::AuthListModel::IsOpRole).toBool();
		checkTrusted = idx.data(net::AuthListModel::IsTrustedRole).toBool();
	}
	QSignalBlocker authOpBlocker(m_ui->authOp);
	QSignalBlocker authTrustedBlocker(m_ui->authTrusted);
	m_ui->authOp->setEnabled(enableOp);
	m_ui->authTrusted->setEnabled(enableTrusted);
	m_ui->authOp->setChecked(checkOp);
	m_ui->authTrusted->setChecked(checkTrusted);
}

void SessionSettingsDialog::sendSessionConf()
{
	if(!m_sessionconf.isEmpty()) {
		if(m_sessionconf.contains("title") &&
		   parentalcontrols::isNsfmTitle(m_sessionconf["title"].toString()))
			m_sessionconf["nsfm"] = true;

		m_doc->sendSessionConf(m_sessionconf);
		m_sessionconf = QJsonObject();
	}

	if(m_featureTiersChanged) {
		QVector<uint8_t> tiers;
		tiers.reserve(int(DP_FEATURE_COUNT));
		for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
			tiers.append(featureBox(DP_Feature(i))->currentIndex());
		}

		m_doc->sendFeatureAccessLevelChange(tiers);
		m_featureTiersChanged = false;
	}

	if(m_featureLimitsChanged) {
		QVector<int32_t> limits;
		limits.reserve(int(DP_FEATURE_LIMIT_COUNT) * int(DP_ACCESS_TIER_COUNT));
		for(int i = 0; i < DP_FEATURE_LIMIT_COUNT; ++i) {
			int value = limitSliderValue(DP_FeatureLimit(i));
			for(int j = 0; j < DP_ACCESS_TIER_COUNT; ++j) {
				limits.append(
					int32_t(j == DP_ACCESS_TIER_OPERATOR ? -1 : value));
			}
		}

		m_doc->sendFeatureLimitsChange(limits);
		m_featureLimitsChanged = false;
	}
}

void SessionSettingsDialog::changeSessionConf(
	const QString &key, const QJsonValue &value, bool now)
{
	m_sessionconf[key] = value;
	if(now) {
		m_saveTimer->stop();
		sendSessionConf();
	} else {
		m_saveTimer->start();
	}
}

void SessionSettingsDialog::titleChanged(const QString &title)
{
	changeSessionConf("title", title);
}
void SessionSettingsDialog::maxUsersChanged()
{
	changeSessionConf("maxUserCount", m_ui->maxUsers->value());
}
void SessionSettingsDialog::denyJoinsChanged(bool set)
{
	changeSessionConf("closed", set);
}
void SessionSettingsDialog::authOnlyChanged(bool set)
{
	changeSessionConf("authOnly", set);
}

void SessionSettingsDialog::allowWebChanged(bool allowWeb)
{
	changeSessionConf("allowWeb", allowWeb);
}

void SessionSettingsDialog::autoresetThresholdChanged()
{
	changeSessionConf(
		"resetThreshold", int(m_ui->autoresetThreshold->value() * 1024 * 1024));
}
void SessionSettingsDialog::keepChatChanged(bool set)
{
	changeSessionConf("preserveChat", set);
}
void SessionSettingsDialog::persistenceChanged(bool set)
{
	changeSessionConf("persistent", set);
}
void SessionSettingsDialog::nsfmChanged(bool set)
{
	changeSessionConf("nsfm", set);
}
void SessionSettingsDialog::deputiesChanged(int idx)
{
	changeSessionConf("deputies", idx > 0);
}
void SessionSettingsDialog::idleOverrideChanged(bool idleOverride)
{
	changeSessionConf("idleOverride", idleOverride);
}

void SessionSettingsDialog::changePassword()
{
	QString prompt;
	if(m_doc->isSessionPasswordProtected())
		prompt = tr("Set a new password or leave blank to remove.");
	else
		prompt = tr("Set a password for the session.");

	utils::getInputPassword(
		this, tr("Session Password"), prompt, QString(),
		[this](const QString &newpass) {
			emit joinPasswordChanged(newpass);
			changeSessionConf("password", newpass, true);
		});
}

void SessionSettingsDialog::changeOpword()
{
	QString prompt;
	if(m_doc->isSessionOpword())
		prompt = tr("Set a new password or leave blank to remove.");
	else
		prompt = tr("Set a password for gaining operator status.");

	utils::getInputPassword(
		this, tr("Operator Password"), prompt, QString(),
		[this](const QString &newpass) {
			changeSessionConf("opword", newpass, true);
		});
}

void SessionSettingsDialog::updateBanImportExportState()
{
	m_ui->importButton->setEnabled(
		m_op && (m_canCryptImpExBans || m_canModImportBans));
	m_ui->exportButton->setEnabled(
		m_op && (m_canCryptImpExBans || m_canModExportBans));
}

void SessionSettingsDialog::importBans()
{
	if(!m_op || !(m_canModImportBans || m_canCryptImpExBans) ||
	   m_awaitingImportExportResponse) {
		return;
	}

	FileWrangler(this).openSessionBans(
		[this](const QString *path, const QByteArray *bytes) {
			QString bans;
			if(bytes) {
				bans = QString::fromUtf8(*bytes);
			} else if(path) {
				QFile file(*path);
				if(!file.open(QFile::ReadOnly)) {
					utils::showCritical(
						this, tr("Session Ban Import"),
						tr("Error opening '%1': %2")
							.arg(*path, file.errorString()));
					return;
				}
				bans = QString::fromUtf8(file.readAll());
				file.close();
			}

			QString errorMessage;
			if(!checkBanImport(bans, errorMessage)) {
				utils::showCritical(
					this, tr("Session Ban Import"), errorMessage);
				return;
			}

			m_awaitingImportExportResponse = true;
			updateBanImportExportState();
			emit requestBanImport(bans);
		});
}

bool SessionSettingsDialog::checkBanImport(
	const QString &bans, QString &outErrorMessage) const
{
	static QRegularExpression cryptRe(
		QStringLiteral("\\Ac[a-zA-Z0-9+/=]+:[a-zA-Z0-9+/=]+\\z"));
	if(cryptRe.match(bans).hasMatch()) {
		if(m_canCryptImpExBans) {
			return true;
		} else {
			outErrorMessage =
				tr("This server does not support importing encrypted bans.");
			return false;
		}
	}

	static QRegularExpression plainRe(QStringLiteral("\\Ap[a-zA-Z0-9+/=]+\\z"));
	if(plainRe.match(bans).hasMatch()) {
		if(m_canModImportBans) {
			return true;
		} else {
			outErrorMessage =
				tr("This server does not support importing plain bans.");
			return false;
		}
	}

	outErrorMessage = tr("File does not appear to contain any ban data.");
	return false;
}

void SessionSettingsDialog::exportBans()
{
	if(!m_op || !(m_canModExportBans || m_canCryptImpExBans) ||
	   m_awaitingImportExportResponse) {
		return;
	}

	if(m_canModExportBans) {
		if(m_canCryptImpExBans) {
			QMessageBox *messageBox = utils::makeMessage(
				this, tr("Choose Ban Export Type"),
				tr("Since you are a moderator, you can export bans encrypted "
				   "or plain. Encrypted bans can only be imported on this "
				   "server. Which format do you want to export?"),
				QString(), QMessageBox::Question, QMessageBox::Cancel);
			QPushButton *encryptedButton =
				messageBox->addButton(tr("Encrypted"), QMessageBox::ActionRole);
			QPushButton *plainButton =
				messageBox->addButton(tr("Plain"), QMessageBox::ActionRole);
			connect(
				messageBox, &QMessageBox::finished, messageBox,
				[this, messageBox, encryptedButton, plainButton] {
					if(messageBox->clickedButton() == encryptedButton) {
						startBanExport(false);
					} else if(messageBox->clickedButton() == plainButton) {
						startBanExport(true);
					}
				});
			messageBox->show();
		} else {
			startBanExport(true);
		}
	} else {
		startBanExport(false);
	}
}

void SessionSettingsDialog::startBanExport(bool plain)
{
	m_awaitingImportExportResponse = true;
	updateBanImportExportState();
	emit requestBanExport(plain);
}


void SessionSettingsDialog::authListChangeOp(bool op)
{
	authListChangeParam(QStringLiteral("o"), op);
}

void SessionSettingsDialog::authListChangeTrusted(bool trusted)
{
	authListChangeParam(QStringLiteral("t"), trusted);
}

void SessionSettingsDialog::importAuthList()
{
	if(!m_op) {
		return;
	}

	FileWrangler(this).openAuthList(
		[this](const QString *path, const QByteArray *bytes) {
			QByteArray content;
			if(bytes) {
				content = *bytes;
			} else if(path) {
				QFile file(*path);
				if(!file.open(QFile::ReadOnly)) {
					utils::showCritical(
						this, tr("Role Import"),
						tr("Error opening '%1': %2")
							.arg(*path, file.errorString()));
					return;
				}
				content = file.readAll();
				file.close();
			}

			QJsonArray list;
			if(!readAuthListImport(content, list)) {
				utils::showCritical(
					this, tr("Role Import"),
					tr("File does not contain a valid role export."));
				return;
			}

			importAuthListChunked(list);
		});
}

void SessionSettingsDialog::exportAuthList()
{
	if(!m_op) {
		return;
	}

	QJsonArray list;
	for(const net::AuthListEntry &e : m_doc->authList()->entries()) {
		if(e.op || e.trusted) {
			QJsonObject o = {
				{QStringLiteral("a"), e.authId},
				{QStringLiteral("u"), e.username},
			};
			if(e.op) {
				o[QStringLiteral("o")] = true;
			}
			if(e.trusted) {
				o[QStringLiteral("t")] = true;
			}
			list.append(o);
		}
	}

	QByteArray content =
		authExportPrefix +
		qCompress(QJsonDocument(list).toJson(QJsonDocument::Compact));
	QString error;
	if(!FileWrangler(this).saveAuthList(content, &error)) {
		utils::showCritical(
			this, tr("Role Export"), tr("Error saving roles: %1").arg(error));
	}
}

bool SessionSettingsDialog::readAuthListImport(
	const QByteArray &content, QJsonArray &outList) const
{
	if(!content.startsWith(authExportPrefix)) {
		return false;
	}

	QJsonDocument doc = QJsonDocument::fromJson(
		qUncompress(content.mid(authExportPrefix.size())));
	if(!doc.isArray()) {
		return false;
	}

	for(const QJsonValue &value : doc.array()) {
		QString authId = value[QStringLiteral("a")].toString();
		if(authId.isEmpty()) {
			return false;
		}

		bool op = value[QStringLiteral("o")].toBool();
		bool trusted = value[QStringLiteral("t")].toBool();
		if(!op && !trusted) {
			return false;
		}

		QJsonObject o = {
			{QStringLiteral("a"), authId},
			{QStringLiteral("u"), value[QStringLiteral("u")].toString()},
		};
		if(op) {
			o[QStringLiteral("o")] = true;
		}
		if(trusted) {
			o[QStringLiteral("t")] = true;
		}
		outList.append(o);
	}

	return true;
}

void SessionSettingsDialog::importAuthListChunked(const QJsonArray &list)
{
	setEnabled(false);
	QApplication::setOverrideCursor(Qt::WaitCursor);
	AuthListImport *import = new AuthListImport(list);
	QTimer *timer = new QTimer(this);
	timer->setInterval(2000);
	timer->setSingleShot(false);
	connect(timer, &QTimer::timeout, this, [this, import, timer]() {
		if(!import->isDone()) {
			if(import->isEmpty()) {
				import->markAsDone();
				timer->stop();
				timer->deleteLater();
				utils::showInformation(
					this, tr("Role Import"),
					tr("%n role(s) imported.", "", import->count()));
			} else {
				emit requestUpdateAuthList(import->takeFirst());
			}
		}
	});
	connect(timer, &QTimer::destroyed, this, [import, this]() {
		delete import;
		setEnabled(true);
		QApplication::restoreOverrideCursor();
	});
	timer->start();
}

QModelIndex SessionSettingsDialog::getSelectedAuthListEntry()
{
	if(m_op) {
		QModelIndexList selected =
			m_ui->authListView->selectionModel()->selectedRows();
		if(selected.size() == 1) {
			return selected.at(0);
		}
	}
	return QModelIndex();
}

void SessionSettingsDialog::authListChangeParam(const QString &key, bool value)
{
	QModelIndex idx = getSelectedAuthListEntry();
	if(idx.isValid() && !idx.data(net::AuthListModel::IsModRole).toBool()) {
		QString authId = idx.data(net::AuthListModel::AuthIdRole).toString();
		emit requestUpdateAuthList({QJsonObject{
			{QStringLiteral("a"), authId},
			{key, value},
		}});
	}
}

}
