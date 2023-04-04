// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/sessionsettings.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/net/banlistmodel.h"
#include "libclient/net/announcementlist.h"
#include "libclient/document.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include "ui_sessionsettings.h"

#include <QDebug>
#include <QStringListModel>
#include <QMenu>
#include <QTimer>
#include <QInputDialog>
#include <QFile>
#include <QJsonDocument>

namespace dialogs {

SessionSettingsDialog::SessionSettingsDialog(Document *doc, QWidget *parent)
	: QDialog(parent), m_ui(new Ui_SessionSettingsDialog), m_doc(doc)
{
	Q_ASSERT(doc);
	m_ui->setupUi(this);

	initPermissionComboBoxes();

	connect(m_doc, &Document::canvasChanged, this, &SessionSettingsDialog::onCanvasChanged);

	// Set up the settings page
	m_saveTimer = new QTimer(this);
	m_saveTimer->setSingleShot(true);
	m_saveTimer->setInterval(1000);
	connect(m_saveTimer, &QTimer::timeout, this, &SessionSettingsDialog::sendSessionConf);

	connect(m_ui->title, &QLineEdit::textEdited, this, &SessionSettingsDialog::titleChanged);
	connect(m_ui->maxUsers, &QSpinBox::editingFinished, this, &SessionSettingsDialog::maxUsersChanged);
	connect(m_ui->denyJoins, &QCheckBox::clicked, this, &SessionSettingsDialog::denyJoinsChanged);
	connect(m_ui->authOnly, &QCheckBox::clicked, this, &SessionSettingsDialog::authOnlyChanged);
	connect(m_ui->autoresetThreshold, &QDoubleSpinBox::editingFinished, this, &SessionSettingsDialog::autoresetThresholdChanged);
	connect(m_ui->preserveChat, &QCheckBox::clicked, this, &SessionSettingsDialog::keepChatChanged);
	connect(m_ui->persistent, &QCheckBox::clicked, this, &SessionSettingsDialog::persistenceChanged);
	connect(m_ui->nsfm, &QCheckBox::clicked, this, &SessionSettingsDialog::nsfmChanged);
	connect(m_ui->deputies, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SessionSettingsDialog::deputiesChanged);

	connect(m_ui->sessionPassword, &QLabel::linkActivated, this, &SessionSettingsDialog::changePassword);
	connect(m_ui->opword, &QLabel::linkActivated, this, &SessionSettingsDialog::changeOpword);

	connect(m_doc, &Document::sessionTitleChanged, m_ui->title, &QLineEdit::setText);
	connect(m_doc, &Document::sessionPreserveChatChanged, m_ui->preserveChat, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionPersistentChanged, m_ui->persistent, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionClosedChanged, m_ui->denyJoins, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionAuthOnlyChanged, this, [this](bool authOnly) {
		m_ui->authOnly->setEnabled(m_op && (authOnly || m_isAuth));
		m_ui->authOnly->setChecked(authOnly);
	});
	connect(m_doc, &Document::sessionPasswordChanged, this, [this](bool hasPassword) {
		m_ui->sessionPassword->setProperty("haspass", hasPassword);
		updatePasswordLabel(m_ui->sessionPassword);
	});
	connect(m_doc, &Document::sessionOpwordChanged, this, [this](bool hasPassword) {
		m_ui->opword->setProperty("haspass", hasPassword);
		updatePasswordLabel(m_ui->opword);
	});
	connect(m_doc, &Document::sessionNsfmChanged, this, &SessionSettingsDialog::updateNsfmCheckbox);
	connect(m_doc, &Document::sessionForceNsfmChanged, this, &SessionSettingsDialog::updateNsfmCheckbox);
	connect(m_doc, &Document::sessionDeputiesChanged, this, [this](bool deputies) { m_ui->deputies->setCurrentIndex(deputies ? 1 : 0); });
	connect(m_doc, &Document::sessionMaxUserCountChanged, m_ui->maxUsers, &QSpinBox::setValue);
	connect(m_doc, &Document::sessionResetThresholdChanged, m_ui->autoresetThreshold, &QDoubleSpinBox::setValue);
	connect(m_doc, &Document::baseResetThresholdChanged, this, [this](int threshold) {
		m_ui->baseResetThreshold->setText(QStringLiteral("+ %1 MB").arg(threshold/(1024.0*1024.0), 0, 'f', 1));
	});
	connect(m_doc, &Document::compatibilityModeChanged, this, &SessionSettingsDialog::setCompatibilityMode);

	// Set up permissions tab
	connect(m_ui->permissionPresets, &widgets::PresetSelector::saveRequested, this, &SessionSettingsDialog::permissionPresetSaving);
	connect(m_ui->permissionPresets, &widgets::PresetSelector::loadRequested, this, &SessionSettingsDialog::permissionPresetSelected);

	// Set up banlist tab
	m_ui->banlistView->setModel(doc->banlist());
	connect(m_ui->removeBan, &QPushButton::clicked, [this]() {
		const int id = m_ui->banlistView->selectionModel()->currentIndex().data(Qt::UserRole).toInt();
		if(id>0) {
			qDebug() << "requesting removal of in-session ban entry" << id;
			m_doc->sendUnban(id);
		}
	});

	// Set up announcements tab
	m_ui->announcementTableView->setModel(doc->announcementList());
	QHeaderView *announcementHeader = m_ui->announcementTableView->horizontalHeader();
	announcementHeader->setSectionResizeMode(0, QHeaderView::Stretch);

	QMenu *addAnnouncementMenu = new QMenu(this);
	QMenu *addPrivateAnnouncementMenu = new QMenu(this);

	m_ui->addAnnouncement->setMenu(addAnnouncementMenu);
	m_ui->addPrivateAnnouncement->setMenu(addPrivateAnnouncementMenu);

	connect(addAnnouncementMenu, &QMenu::triggered, [this](QAction *a) {
		const QString apiUrl = a->property("API_URL").toString();
		qDebug() << "Requesting public announcement:" << apiUrl;
		m_doc->sendAnnounce(apiUrl, false);
	});
	connect(addPrivateAnnouncementMenu, &QMenu::triggered, [this](QAction *a) {
		const QString apiUrl = a->property("API_URL").toString();
		qDebug() << "Requesting private announcement:" << apiUrl;
		m_doc->sendAnnounce(apiUrl, true);
	});

	connect(m_ui->removeAnnouncement, &QPushButton::clicked, [this]() {
		auto sel = m_ui->announcementTableView->selectionModel()->selection();
		QString apiUrl;
		if(!sel.isEmpty())
			apiUrl = sel.first().indexes().first().data(Qt::UserRole).toString();
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
	setCompatibilityMode(m_doc->isCompatibilityMode());
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

void SessionSettingsDialog::reloadSettings()
{
	const auto listservers = sessionlisting::ListServerModel::listServers(false);
	auto *addAnnouncementMenu = m_ui->addAnnouncement->menu();
	auto *addPrivateAnnouncementMenu = m_ui->addPrivateAnnouncement->menu();

	addAnnouncementMenu->clear();
	addPrivateAnnouncementMenu->clear();

	for(const auto &listserver : listservers) {
		if(listserver.publicListings) {
			QAction *a = addAnnouncementMenu->addAction(listserver.icon, listserver.name);
			a->setProperty("API_URL", listserver.url);
		}

		if(listserver.privateListings) {
			QAction *a2 = addPrivateAnnouncementMenu->addAction(listserver.icon, listserver.name);
			a2->setProperty("API_URL", listserver.url);
		}
	}

	m_ui->addAnnouncement->setEnabled(!addAnnouncementMenu->isEmpty());
	m_ui->addPrivateAnnouncement->setEnabled(!addPrivateAnnouncementMenu->isEmpty());
}

void SessionSettingsDialog::setPersistenceEnabled(bool enable)
{
	m_ui->persistent->setEnabled(m_op && enable);
	m_canPersist = enable;
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
	m_ui->authOnly->setEnabled(m_op && (m_isAuth || m_ui->authOnly->isChecked()));
}

void SessionSettingsDialog::onCanvasChanged(canvas::CanvasModel *canvas)
{
	if(!canvas)
		return;

	canvas::AclState *acl = canvas->aclState();

	connect(acl, &canvas::AclState::localOpChanged, this, &SessionSettingsDialog::onOperatorModeChanged);
	connect(acl, &canvas::AclState::featureTiersChanged, this, &SessionSettingsDialog::onFeatureTiersChanged);

	onOperatorModeChanged(acl->amOperator());
	onFeatureTiersChanged(acl->featureTiers());

}

void SessionSettingsDialog::onOperatorModeChanged(bool op)
{
	QWidget *w[] = {
		m_ui->title,
		m_ui->maxUsers,
		m_ui->denyJoins,
		m_ui->preserveChat,
		m_ui->nsfm,
		m_ui->deputies,
		m_ui->sessionPassword,
		m_ui->opword,
		m_ui->addAnnouncement,
		m_ui->removeAnnouncement,
		m_ui->removeBan
	};
	m_op = op;
	for(unsigned int i=0;i<sizeof(w)/sizeof(*w);++i)
		w[i]->setEnabled(op);

	for(int i = 0; i < DP_FEATURE_COUNT; ++i)
		featureBox(DP_Feature(i))->setEnabled(op);

	m_ui->persistent->setEnabled(m_canPersist && op);
	m_ui->autoresetThreshold->setEnabled(m_canAutoreset && op);
	m_ui->authOnly->setEnabled(op && (m_isAuth || m_ui->authOnly->isChecked()));
	m_ui->permissionPresets->setWriteOnly(!op);
	updatePasswordLabel(m_ui->sessionPassword);
	updatePasswordLabel(m_ui->opword);
	setCompatibilityMode(m_doc->isCompatibilityMode());
}

QComboBox *SessionSettingsDialog::featureBox(DP_Feature f)
{
	switch(f) {
	case DP_FEATURE_PUT_IMAGE: return m_ui->permPutImage;
	case DP_FEATURE_REGION_MOVE: return m_ui->permRegionMove;
	case DP_FEATURE_RESIZE: return m_ui->permResize;
	case DP_FEATURE_BACKGROUND: return m_ui->permBackground;
	case DP_FEATURE_EDIT_LAYERS: return m_ui->permEditLayers;
	case DP_FEATURE_OWN_LAYERS: return m_ui->permOwnLayers;
	case DP_FEATURE_CREATE_ANNOTATION: return m_ui->permCreateAnnotation;
	case DP_FEATURE_LASER: return m_ui->permLaser;
	case DP_FEATURE_UNDO: return m_ui->permUndo;
	case DP_FEATURE_METADATA: return m_ui->permMetadata;
	case DP_FEATURE_TIMELINE: return m_ui->permTimeline;
	case DP_FEATURE_MYPAINT: return m_ui->permMyPaint;
	default: Q_ASSERT_X(false, "featureBox", "unhandled case"); return nullptr;
	}
}
void SessionSettingsDialog::onFeatureTiersChanged(const DP_FeatureTiers &features)
{
	m_ui->permPutImage->setCurrentIndex(int(features.tiers[DP_FEATURE_PUT_IMAGE]));
	m_ui->permRegionMove->setCurrentIndex(int(features.tiers[DP_FEATURE_REGION_MOVE]));
	m_ui->permResize->setCurrentIndex(int(features.tiers[DP_FEATURE_RESIZE]));
	m_ui->permBackground->setCurrentIndex(int(features.tiers[DP_FEATURE_BACKGROUND]));
	m_ui->permEditLayers->setCurrentIndex(int(features.tiers[DP_FEATURE_EDIT_LAYERS]));
	m_ui->permOwnLayers->setCurrentIndex(int(features.tiers[DP_FEATURE_OWN_LAYERS]));
	m_ui->permCreateAnnotation->setCurrentIndex(int(features.tiers[DP_FEATURE_CREATE_ANNOTATION]));
	m_ui->permLaser->setCurrentIndex(int(features.tiers[DP_FEATURE_LASER]));
	m_ui->permUndo->setCurrentIndex(int(features.tiers[DP_FEATURE_UNDO]));
	m_ui->permMetadata->setCurrentIndex(int(features.tiers[DP_FEATURE_METADATA]));
	m_ui->permTimeline->setCurrentIndex(int(features.tiers[DP_FEATURE_TIMELINE]));
	m_ui->permMyPaint->setCurrentIndex(int(features.tiers[DP_FEATURE_MYPAINT]));
}

void SessionSettingsDialog::initPermissionComboBoxes()
{
	// Note: these must match the canvas::Tier enum
	const QString items[] = {
		tr("Operators"),
		tr("Trusted"),
		tr("Registered"),
		tr("Everyone")
	};

	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		QComboBox *box = featureBox(DP_Feature(i));
		for(uint j = 0; j < sizeof(items) / sizeof(QString); ++j)
			box->addItem(items[j]);

		box->setProperty("featureIdx", i);
		connect(box, QOverload<int>::of(&QComboBox::activated), this, &SessionSettingsDialog::permissionChanged);
	}
}

void SessionSettingsDialog::permissionChanged()
{
	m_featureTiersChanged = true;
	m_saveTimer->start();
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
			cfg.value(box->objectName()).toInt(box->currentIndex())
			);
	}
	permissionChanged();

	// Deputies
	{
		auto *box = m_ui->deputies;
		box->setCurrentIndex(
			cfg.value(box->objectName()).toInt(box->currentIndex())
			);
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
	if(!f.open(QFile::WriteOnly)) {
		qWarning("%s: could not open file", qPrintable(presetFile));
		return;
	}
	f.write(QJsonDocument(cfg).toJson());
}

void SessionSettingsDialog::updatePasswordLabel(QLabel *label)
{
	bool hasPass = label->property("haspass").toBool();
	QString txt = QStringLiteral("<b>%1</b>").arg(
		hasPass ? tr("yes", "password") : tr("no", "password"));

	if(m_op) {
		txt.append(QStringLiteral(" (<a href=\"#\">%1</a>)").arg(
			hasPass ? tr("change", "password") : tr("assign", "password")));
	}

	label->setText(txt);
}

void SessionSettingsDialog::updateNsfmCheckbox(bool)
{
	if(m_doc->isSessionForceNsfm()) {
		m_ui->nsfm->setEnabled(false);
		m_ui->nsfm->setChecked(true);
	} else {
		m_ui->nsfm->setEnabled(true);
		m_ui->nsfm->setChecked(m_doc->isSessionNsfm());
	}
}

void SessionSettingsDialog::setCompatibilityMode(bool compatibilityMode)
{
	if(compatibilityMode) {
		m_ui->permMyPaint->setEnabled(false);
		m_ui->permTimeline->setEnabled(false);
		m_ui->permMetadata->setEnabled(false);
	}
}

void SessionSettingsDialog::sendSessionConf()
{
	if(!m_sessionconf.isEmpty()) {
		if(m_sessionconf.contains("title") && parentalcontrols::isNsfmTitle(m_sessionconf["title"].toString()))
			m_sessionconf["nsfm"] = true;

		m_doc->sendSessionConf(m_sessionconf);
		m_sessionconf = QJsonObject();
	}

	if(m_featureTiersChanged) {
		uint8_t tiers[DP_FEATURE_COUNT];
		for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
			tiers[i] = featureBox(DP_Feature(i))->currentIndex();
		}

		m_doc->sendFeatureAccessLevelChange(tiers);
		m_featureTiersChanged = false;
	}
}

void SessionSettingsDialog::changeSesionConf(const QString &key, const QJsonValue &value, bool now)
{
	m_sessionconf[key] = value;
	if(now) {
		m_saveTimer->stop();
		sendSessionConf();
	} else {
		m_saveTimer->start();
	}
}

void SessionSettingsDialog::titleChanged(const QString &title) { changeSesionConf("title", title); }
void SessionSettingsDialog::maxUsersChanged() { changeSesionConf("maxUserCount", m_ui->maxUsers->value()); }
void SessionSettingsDialog::denyJoinsChanged(bool set) { changeSesionConf("closed", set); }
void SessionSettingsDialog::authOnlyChanged(bool set) { changeSesionConf("authOnly", set); }

void SessionSettingsDialog::autoresetThresholdChanged() { changeSesionConf("resetThreshold", int(m_ui->autoresetThreshold->value()* 1024 * 1024)); }
void SessionSettingsDialog::keepChatChanged(bool set) { changeSesionConf("preserveChat", set); }
void SessionSettingsDialog::persistenceChanged(bool set) { changeSesionConf("persistent", set); }
void SessionSettingsDialog::nsfmChanged(bool set) { changeSesionConf("nsfm", set); }
void SessionSettingsDialog::deputiesChanged(int idx) { changeSesionConf("deputies", idx>0); }

void SessionSettingsDialog::changePassword()
{
	QString prompt;
	if(m_doc->isSessionPasswordProtected())
		prompt = tr("Set a new password or leave blank to remove.");
	else
		prompt = tr("Set a password for the session.");

	bool ok;
	QString newpass = QInputDialog::getText(
				this,
				tr("Session Password"),
				prompt,
				QLineEdit::Password,
				QString(),
				&ok
	);
	if(ok)
		changeSesionConf("password", newpass, true);
}

void SessionSettingsDialog::changeOpword()
{
	QString prompt;
	if(m_doc->isSessionOpword())
		prompt = tr("Set a new password or leave blank to remove.");
	else
		prompt = tr("Set a password for gaining operator status.");

	bool ok;
	QString newpass = QInputDialog::getText(
				this,
				tr("Operator Password"),
				prompt,
				QLineEdit::Password,
				QString(),
				&ok
	);
	if(ok)
		changeSesionConf("opword", newpass, true);
}

}
