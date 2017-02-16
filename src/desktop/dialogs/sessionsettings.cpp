/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "sessionsettings.h"
#include "utils/listservermodel.h"
#include "net/banlistmodel.h"
#include "document.h"
#include "../shared/net/meta2.h"
#include "canvas/canvasmodel.h"
#include "canvas/aclfilter.h"
#include "parentalcontrols/parentalcontrols.h"

#include "ui_sessionsettings.h"

#include <QDebug>
#include <QStringListModel>
#include <QIdentityProxyModel>
#include <QMenu>
#include <QTimer>
#include <QInputDialog>

namespace dialogs {

/**
 * @brief A proxy model that makes public listing server URL list nicer
 *
 * If the listing server is known, replace the URL with its name and icon
 */
class ListingProxyModel : public QIdentityProxyModel {
public:
	ListingProxyModel(QObject *parent=nullptr)
		: QIdentityProxyModel(parent)
	{
		sessionlisting::ListServerModel servermodel(false);
		for(const sessionlisting::ListServer &s : servermodel.servers()) {
			m_servers[s.url] = QPair<QIcon,QString>(s.icon, s.name);
		}
	}

	QVariant data(const QModelIndex &proxyIndex, int role) const override
	{
		QVariant data = QAbstractProxyModel::data(proxyIndex, role);
		if(role == Qt::DisplayRole && data.isValid()) {
			QString url = data.toString();
			if(m_servers.contains(url))
				return m_servers[url].second;

		} else if(role == Qt::DecorationRole) {
			QString url = QAbstractProxyModel::data(proxyIndex, Qt::DisplayRole).toString();
			if(m_servers.contains(url))
				return m_servers[url].first;
		} else if(role == Qt::UserRole) {
			return QAbstractProxyModel::data(proxyIndex, Qt::DisplayRole);
		}

		return data;
	}

	Qt::ItemFlags flags(const QModelIndex &index) const override
	{
		if(index.isValid())
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		return 0;
	}

	QHash<QString,QPair<QIcon,QString>> m_servers;
};

SessionSettingsDialog::SessionSettingsDialog(Document *doc, QWidget *parent)
	: QDialog(parent), m_ui(new Ui_SessionSettingsDialog), m_doc(doc),
	  m_aclFlags(0), m_aclMask(0), m_canPersist(false)
{
	Q_ASSERT(doc);
	m_ui->setupUi(this);

	connect(m_doc, &Document::canvasChanged, this, &SessionSettingsDialog::onCanvasChanged);

	// Set up the settings page
	m_saveTimer = new QTimer(this);
	m_saveTimer->setSingleShot(true);
	m_saveTimer->setInterval(1000);
	connect(m_saveTimer, &QTimer::timeout, this, &SessionSettingsDialog::sendSessionConf);

	connect(m_ui->title, &QLineEdit::textEdited, this, &SessionSettingsDialog::titleChanged);
	connect(m_ui->maxUsers, &QSpinBox::editingFinished, this, &SessionSettingsDialog::maxUsersChanged);
	connect(m_ui->denyJoins, &QCheckBox::clicked, this, &SessionSettingsDialog::denyJoinsChanged);
	connect(m_ui->lockNewUsers, &QCheckBox::clicked, this, &SessionSettingsDialog::lockNewUsersChanged);
	connect(m_ui->lockImages, &QCheckBox::clicked, this, &SessionSettingsDialog::lockImagesChanged);
	connect(m_ui->lockAnnotations, &QCheckBox::clicked, this, &SessionSettingsDialog::lockAnnotationsChanged);
	connect(m_ui->lockLayerCtrl, &QCheckBox::clicked, this, &SessionSettingsDialog::lockLayerCtrlChanged);
	connect(m_ui->ownLayers, &QCheckBox::clicked, this, &SessionSettingsDialog::ownLayersChanged);
	connect(m_ui->preserveChat, &QCheckBox::clicked, this, &SessionSettingsDialog::keepChatChanged);
	connect(m_ui->persistent, &QCheckBox::clicked, this, &SessionSettingsDialog::persistenceChanged);
	connect(m_ui->nsfm, &QCheckBox::clicked, this, &SessionSettingsDialog::nsfmChanged);

	connect(m_ui->sessionPassword, &QLabel::linkActivated, this, &SessionSettingsDialog::changePassword);
	connect(m_ui->opword, &QLabel::linkActivated, this, &SessionSettingsDialog::changeOpword);

	connect(m_doc, &Document::sessionTitleChanged, m_ui->title, &QLineEdit::setText);
	connect(m_doc, &Document::sessionPreserveChatChanged, m_ui->preserveChat, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionPersistentChanged, m_ui->persistent, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionClosedChanged, m_ui->denyJoins, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionPasswordChanged, [this](bool hasPassword) {
		m_ui->sessionPassword->setProperty("haspass", hasPassword);
		updatePasswordLabel(m_ui->sessionPassword);
	});
	connect(m_doc, &Document::sessionOpwordChanged, [this](bool hasPassword) {
		m_ui->opword->setProperty("haspass", hasPassword);
		updatePasswordLabel(m_ui->opword);
	});
	connect(m_doc, &Document::sessionNsfmChanged, m_ui->nsfm, &QCheckBox::setChecked);
	connect(m_doc, &Document::sessionMaxUserCountChanged, m_ui->maxUsers, &QSpinBox::setValue);

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
	ListingProxyModel *listingProxy = new ListingProxyModel(this);
	listingProxy->setSourceModel(doc->announcementList());
	m_ui->announcementListView->setModel(listingProxy);

	QMenu *addAnnouncementMenu = new QMenu(this);

	QHashIterator<QString,QPair<QIcon,QString>> i(listingProxy->m_servers);
	while(i.hasNext()) {
		auto item = i.next();
		QAction *a = addAnnouncementMenu->addAction(item.value().first, item.value().second);
		a->setProperty("API_URL", item.key());
	}

	m_ui->addAnnouncement->setMenu(addAnnouncementMenu);

	connect(addAnnouncementMenu, &QMenu::triggered, [this](QAction *a) {
		QString apiUrl = a->property("API_URL").toString();
		qDebug() << "Requesting announcement:" << apiUrl;
		m_doc->sendAnnounce(apiUrl);
	});

	connect(m_ui->removeAnnouncement, &QPushButton::clicked, [this]() {
		auto sel = m_ui->announcementListView->selectionModel()->selection();
		QString apiUrl;
		if(!sel.isEmpty())
			apiUrl = sel.first().indexes().first().data(Qt::UserRole).toString();
		if(!apiUrl.isEmpty()) {
			qDebug() << "Requesting unlisting:" << apiUrl;
			m_doc->sendUnannounce(apiUrl);
		}
	});
}

SessionSettingsDialog::~SessionSettingsDialog()
{
	delete m_ui;
}

void SessionSettingsDialog::setPersistenceEnabled(bool enable)
{
	m_ui->persistent->setEnabled(m_op && enable);
	m_canPersist = enable;
}

void SessionSettingsDialog::onCanvasChanged(canvas::CanvasModel *canvas)
{
	if(!canvas)
		return;

	canvas::AclFilter *acl = canvas->aclFilter();

	connect(acl, &canvas::AclFilter::layerControlLockChanged, m_ui->lockLayerCtrl, &QCheckBox::setChecked);
	connect(acl, &canvas::AclFilter::ownLayersChanged, m_ui->ownLayers, &QCheckBox::setChecked);
	connect(acl, &canvas::AclFilter::imageCmdLockChanged, m_ui->lockImages, &QCheckBox::setChecked);
	connect(acl, &canvas::AclFilter::lockByDefaultChanged, m_ui->lockNewUsers, &QCheckBox::setChecked);
	connect(acl, &canvas::AclFilter::annotationCreationLockChanged, m_ui->lockAnnotations, &QCheckBox::setChecked);
	connect(acl, &canvas::AclFilter::localOpChanged, this, &SessionSettingsDialog::onOperatorModeChanged);
}

void SessionSettingsDialog::onOperatorModeChanged(bool op)
{
	QWidget *w[] = {
		m_ui->title,
		m_ui->maxUsers,
		m_ui->denyJoins,
		m_ui->lockNewUsers,
		m_ui->lockImages,
		m_ui->lockAnnotations,
		m_ui->lockLayerCtrl,
		m_ui->ownLayers,
		m_ui->preserveChat,
		m_ui->nsfm,
		m_ui->sessionPassword,
		m_ui->opword,
		m_ui->addAnnouncement,
		m_ui->removeAnnouncement,
		m_ui->removeBan
	};
	m_op = op;
	for(unsigned int i=0;i<sizeof(w)/sizeof(*w);++i)
		w[i]->setEnabled(op);
	m_ui->persistent->setEnabled(m_canPersist && op);
	updatePasswordLabel(m_ui->sessionPassword);
	updatePasswordLabel(m_ui->opword);
}

void SessionSettingsDialog::updatePasswordLabel(QLabel *label)
{
	QString txt;
	if(m_op)
		txt = QStringLiteral("<b>%1</b> (<a href=\"#\">%2</a>)");
	else
		txt = QStringLiteral("<b>%1</b>");

	if(label->property("haspass").toBool())
		txt = txt.arg(tr("yes", "password"), tr("change", "password"));
	else
		txt = txt.arg(tr("no", "password"), tr("assign", "password"));

	label->setText(txt);
}

void SessionSettingsDialog::sendSessionConf()
{
	if(!m_sessionconf.isEmpty()) {
		if(m_sessionconf.contains("title") && parentalcontrols::isNsfmTitle(m_sessionconf["title"].toString()))
			m_sessionconf["nsfm"] = true;

		m_doc->sendSessionConf(m_sessionconf);
		m_sessionconf = QJsonObject();
	}

	if(m_aclMask | m_aclFlags) {
		m_doc->sendSessionAclChange(m_aclFlags, m_aclMask);
		m_aclFlags = 0;
		m_aclMask = 0;
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

void SessionSettingsDialog::changeSessionAcl(uint16_t flag, bool set)
{
	m_aclMask |= flag;
	if(set)
		m_aclFlags |= flag;
	m_saveTimer->start();
}

void SessionSettingsDialog::titleChanged(const QString &title) { changeSesionConf("title", title); }
void SessionSettingsDialog::maxUsersChanged() { changeSesionConf("maxUserCount", m_ui->maxUsers->value()); }
void SessionSettingsDialog::denyJoinsChanged(bool set) { changeSesionConf("closed", set); }
void SessionSettingsDialog::lockNewUsersChanged(bool set) { changeSessionAcl(protocol::SessionACL::LOCK_DEFAULT, set); }
void SessionSettingsDialog::lockImagesChanged(bool set) { changeSessionAcl(protocol::SessionACL::LOCK_IMAGES, set); }
void SessionSettingsDialog::lockAnnotationsChanged(bool set) { changeSessionAcl(protocol::SessionACL::LOCK_ANNOTATIONS, set); }
void SessionSettingsDialog::lockLayerCtrlChanged(bool set) { changeSessionAcl(protocol::SessionACL::LOCK_LAYERCTRL, set); }
void SessionSettingsDialog::ownLayersChanged(bool set) { changeSessionAcl(protocol::SessionACL::LOCK_OWNLAYERS, set); }

void SessionSettingsDialog::keepChatChanged(bool set) { changeSesionConf("preserveChat", set); }
void SessionSettingsDialog::persistenceChanged(bool set) { changeSesionConf("persistent", set); }
void SessionSettingsDialog::nsfmChanged(bool set) { changeSesionConf("nsfm", set); }

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
