// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/snapshots.h>
#include <dpimpex/load.h>
#include <dpmsg/reset_stream.h>
}
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/document.h"
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/settings.h"
#include "libclient/tools/selection.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/transform.h"
#include "libclient/utils/images.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/functionrunnable.h"
#include "libshared/util/qtcompat.h"
#include <QBuffer>
#include <QDir>
#include <QGuiApplication>
#include <QPainter>
#include <QRunnable>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <QtEndian>
#include <dpcommon/platform_qt.h>
#include <parson.h>
#ifndef HAVE_CLIPBOARD_EMULATION
#	include <QClipboard>
#endif

Document::Document(
	int canvasImplementation, libclient::settings::Settings &settings,
	QObject *parent)
	: QObject(parent)
	, m_canvasImplementation(canvasImplementation)
	, m_resetstate()
	, m_messageBuffer()
	, m_canvas(nullptr)
	, m_settings(settings)
	, m_dirty(false)
	, m_autosave(false)
	, m_canAutosave(false)
	, m_saveInProgress(false)
	, m_wantCanvasHistoryDump(false)
	, m_sessionPersistent(false)
	, m_sessionClosed(false)
	, m_sessionAuthOnly(false)
	, m_sessionAllowWeb(false)
	, m_sessionPreserveChat(false)
	, m_sessionPasswordProtected(false)
	, m_sessionOpword(false)
	, m_sessionNsfm(false)
	, m_sessionForceNsfm(false)
	, m_sessionDeputies(false)
	, m_sessionIdleOverride(false)
	, m_sessionAllowIdleOverride(false)
	, m_sessionMaxUserCount(0)
	, m_sessionHistoryMaxSize(0)
	, m_sessionResetThreshold(0)
	, m_baseResetThreshold(0)
	, m_sessionIdleTimeLimit(0)
	, m_sessionOutOfSpace(false)
	, m_preparingReset(false)
{
	// Initialize
	m_client = new net::Client(this, this);
	m_settings.bindMessageQueueDrainRate(
		m_client, &net::Client::setSmoothDrainRate);
	m_toolctrl = new tools::ToolController(m_client, this);
	m_settings.bindSmoothing(
		m_toolctrl, &tools::ToolController::setGlobalSmoothing);
	m_settings.bindInterpolateInputs(
		m_toolctrl, &tools::ToolController::setInterpolateInputs);
	m_settings.bindMouseSmoothing(
		m_toolctrl, &tools::ToolController::setMouseSmoothing);
	m_banlist = new net::BanlistModel(this);
	m_authList = new net::AuthListModel(this);
	m_announcementlist =
		new net::AnnouncementListModel(settings.listServers(), this);
	m_serverLog = new QStringListModel(this);

	m_autosaveTimer = new QTimer(this);
	m_autosaveTimer->setSingleShot(true);
	connect(m_autosaveTimer, &QTimer::timeout, this, &Document::autosaveNow);

	// Make connections
	connect(
		m_client, &net::Client::serverConnected, this,
		&Document::serverConnected);
	connect(
		m_client, &net::Client::serverLoggedIn, this, &Document::onServerLogin);
	connect(
		m_client, &net::Client::serverDisconnected, this,
		&Document::onServerDisconnect);
	connect(
		m_client, &net::Client::serverDisconnected, this,
		&Document::serverDisconnected);
	connect(
		m_client, &net::Client::serverPreparingReset, this,
		&Document::setPreparingReset);

	connect(
		m_client, &net::Client::needSnapshot, this, &Document::snapshotNeeded);
	connect(
		m_client, &net::Client::lagMeasured, this, &Document::onLagMeasured);
	connect(
		m_client, &net::Client::sessionConfChange, this,
		&Document::onSessionConfChanged);
	connect(
		m_client, &net::Client::autoresetQueried, this,
		&Document::onAutoresetQueried);
	connect(
		m_client, &net::Client::autoresetRequested, this,
		&Document::onAutoresetRequested);
	connect(
		m_client, &net::Client::serverLog, this, &Document::addServerLogEntry);

	connect(
		m_client, &net::Client::sessionResetted, this,
		&Document::onSessionResetted);
	connect(
		m_client, &net::Client::streamResetStarted, this,
		&Document::onStreamResetStarted, Qt::DirectConnection);
	connect(
		m_client, &net::Client::streamResetProgressed, this,
		&Document::onStreamResetProgressed);
	connect(
		m_client, &net::Client::sessionOutOfSpace, this,
		&Document::onSessionOutOfSpace);

	connect(
		this, &Document::justInTimeSnapshotGenerated, this,
		&Document::sendResetSnapshot, Qt::QueuedConnection);
	connect(
		this, &Document::buildStreamResetImageFinished, this,
		&Document::startSendingStreamResetSnapshot, Qt::QueuedConnection);

	connect(
		m_toolctrl, &tools::ToolController::deleteAnnotationRequested, this,
		&Document::deleteAnnotation);
}

void Document::initCanvas()
{
	delete m_canvas;

	m_canvas = new canvas::CanvasModel{
		m_settings,
		m_client->myId(),
		m_canvasImplementation,
		m_settings.engineFrameRate(),
		m_settings.engineSnapshotCount(),
		m_settings.engineSnapshotInterval() * 1000LL,
		m_wantCanvasHistoryDump,
		this};

	m_toolctrl->setModel(m_canvas);

	connect(
		m_canvas, &canvas::CanvasModel::canvasModified, this,
		&Document::markDirty);
	connect(
		m_canvas->layerlist(), &canvas::LayerListModel::moveRequested, this,
		&Document::onMoveLayerRequested);

	connect(
		m_canvas, &canvas::CanvasModel::titleChanged, this,
		&Document::sessionTitleChanged);
	connect(
		m_canvas, &canvas::CanvasModel::recorderStateChanged, this,
		&Document::recorderStateChanged);
	connect(
		m_canvas, &canvas::CanvasModel::permissionDenied, this,
		&Document::permissionDenied);

	connect(
		m_canvas->paintEngine(), &canvas::PaintEngine::streamResetStarted, this,
		&Document::buildStreamResetImage, Qt::QueuedConnection);
	connect(
		m_client, &net::Client::catchupProgress, m_canvas->paintEngine(),
		&canvas::PaintEngine::enqueueCatchupProgress);
	connect(
		m_canvas->paintEngine(), &canvas::PaintEngine::caughtUpTo, this,
		&Document::catchupProgress, Qt::QueuedConnection);

	connect(
		m_canvas->aclState(), &canvas::AclState::localOpChanged, m_authList,
		&net::AuthListModel::setIsOperator);

	emit canvasChanged(m_canvas);
	m_canvas->paintEngine()->resetAcl(m_client->myId());

	setCurrentPath(QString(), DP_SAVE_IMAGE_UNKNOWN);
}

void Document::onSessionResetted()
{
	Q_ASSERT(m_canvas);
	setSessionOutOfSpace(false);
	setPreparingReset(false);
	if(!m_canvas) {
		qWarning("sessionResetted: no canvas!");
		return;
	}

	// Cancel any possibly active local drawing process.
	// This prevents jumping lines across the canvas if it shifts.
	m_toolctrl->cancelMultipartDrawing();
	m_toolctrl->endDrawing(false, false);

	emit sessionResetState(m_canvas->paintEngine()->viewCanvasState());

	// Clear out the canvas in preparation for the new data that is about to
	// follow
	m_canvas->resetCanvas();
	m_resetstate.clear();
}

void Document::onSessionOutOfSpace()
{
	setSessionOutOfSpace(true);
}

bool Document::loadBlank(const QSize &size, const QColor &background)
{
	setAutosave(false);
	initCanvas();
	unmarkDirty();

	m_canvas->loadBlank(m_settings.engineUndoDepth(), size, background);
	setCurrentPath(QString(), DP_SAVE_IMAGE_UNKNOWN);
	return true;
}

void Document::loadState(
	const drawdance::CanvasState &canvasState, const QString &path,
	DP_SaveImageType type, bool dirty)
{
	setAutosave(false);
	initCanvas();
	if(dirty) {
		markDirty();
	} else {
		unmarkDirty();
	}
	m_canvas->loadCanvasState(m_settings.engineUndoDepth(), canvasState);
	setCurrentPath(path, type);
}

static bool isSessionTemplate(DP_Player *player)
{
	JSON_Value *header = DP_player_header(player);
	return header &&
		   DP_str_equal(
			   json_object_get_string(json_value_get_object(header), "type"),
			   "template");
}

DP_LoadResult Document::loadRecording(
	const QString &path, bool debugDump, bool *outIsTemplate)
{
	DP_LoadResult result;
	DP_Player *player =
		debugDump ? DP_load_debug_dump(path.toUtf8().constData(), &result)
				  : DP_load_recording(path.toUtf8().constData(), &result);
	bool isTemplate;
	switch(result) {
	case DP_LOAD_RESULT_SUCCESS:
		setAutosave(false);
		initCanvas();
		unmarkDirty();
		m_canvas->loadPlayer(player);
		// Session templates are played back to the end instantly. They're only
		// supposed to contain a reset snapshot, so should be very quick.
		isTemplate = isSessionTemplate(player);
		if(isTemplate) {
			m_canvas->paintEngine()->flushPlayback();
			m_canvas->paintEngine()->closePlayback();
		}
		setCurrentPath(path, DP_SAVE_IMAGE_UNKNOWN);
		break;
	default:
		Q_ASSERT(!player);
		isTemplate = false;
		break;
	}
	if(outIsTemplate) {
		*outIsTemplate = isTemplate;
	}
	return result;
}

void Document::onServerLogin(
	bool join, bool compatibilityMode, const QString &joinPassword,
	const QString &authId)
{
	if(join)
		initCanvas();

	Q_ASSERT(m_canvas);

	m_canvas->connectedToServer(m_client->myId(), join, compatibilityMode);
	m_banlist->setShowSensitive(m_client->isModerator());
	m_authList->setOwnAuthId(authId);

	if(!m_recordOnConnect.isEmpty()) {
		m_originalRecordingFilename = m_recordOnConnect;
		startRecording(m_recordOnConnect);
		m_recordOnConnect = QString();
	}

	m_sessionHistoryMaxSize = 0;
	m_baseResetThreshold = 0;
	emit serverLoggedIn(join, joinPassword);
	emit compatibilityModeChanged(compatibilityMode);
}

void Document::onServerDisconnect()
{
	if(m_canvas) {
		m_canvas->disconnectedFromServer();
		m_canvas->setTitle(QString());
	}
	m_banlist->clear();
	m_authList->clear();
	m_announcementlist->clear();
	setSessionOpword(false);
	setSessionOutOfSpace(false);
	emit compatibilityModeChanged(false);
	setPreparingReset(false);
	m_autoResetCorrelator = QString();
	setStreamResetState(StreamResetState::None);
	m_pingHistory.clear();
}

void Document::setPreparingReset(bool preparing)
{
	if(preparing != m_preparingReset) {
		m_preparingReset = preparing;
		emit preparingResetChanged(preparing);
	}
}

void Document::onLagMeasured(qint64 msec)
{
	if(m_client->isConnected()) {
		while(m_pingHistory.size() >= 64) {
			m_pingHistory.dequeue();
		}
		m_pingHistory.enqueue(qMax(qint64(1), msec));
	}
}

void Document::onSessionConfChanged(const QJsonObject &config)
{
	if(config.contains("persistent"))
		setSessionPersistent(config["persistent"].toBool());

	if(config.contains("closed"))
		setSessionClosed(config["closed"].toBool());

	if(config.contains("authOnly"))
		setSessionAuthOnly(config["authOnly"].toBool());

	if(config.contains("title"))
		m_canvas->setTitle(config["title"].toString());

	if(config.contains("preserveChat"))
		setSessionPreserveChat(config["preserveChat"].toBool());

	if(config.contains("hasPassword"))
		setSessionPasswordProtected(config["hasPassword"].toBool());

	if(config.contains("hasOpword"))
		setSessionOpword(config["hasOpword"].toBool());

	if(config.contains("nsfm"))
		setSessionNsfm(config["nsfm"].toBool());

	if(config.contains("forceNsfm"))
		setSessionForceNsfm(config["forceNsfm"].toBool());

	if(config.contains("deputies"))
		setSessionDeputies(config["deputies"].toBool());

	bool idleConfigSet = false;
	if(config.contains("idleTimeLimit")) {
		idleConfigSet = true;
		setSessionIdleTimeLimit(config["idleTimeLimit"].toInt());
	}
	if(config.contains("idleOverride")) {
		idleConfigSet = true;
		setSessionIdleOverride(config["idleOverride"].toBool());
	}
	if(config.contains("allowIdleOverride")) {
		idleConfigSet = true;
		setSessionAllowIdleOverride(config["allowIdleOverride"].toBool());
	}
	if(idleConfigSet) {
		emit sessionIdleChanged(
			m_sessionIdleTimeLimit, m_sessionIdleOverride,
			m_sessionAllowIdleOverride && m_client->isModerator());
	}

	if(config.contains("allowWeb")) {
		setSessionAllowWeb(config["allowWeb"].toBool());
	}

	if(config.contains("maxUserCount"))
		setSessionMaxUserCount(config["maxUserCount"].toInt());

	if(config.contains("resetThreshold"))
		setSessionResetThreshold(config["resetThreshold"].toInt());

	if(config.contains("resetThresholdBase"))
		setBaseResetThreshold(config["resetThresholdBase"].toInt());

	if(config.contains("banlist"))
		m_banlist->updateBans(config["banlist"].toArray());

	if(config.contains("muted"))
		m_canvas->userlist()->updateMuteList(config["muted"].toArray());

	if(config.contains("announcements")) {
		m_announcementlist->clear();
		QString jc;
		for(auto v : config["announcements"].toArray()) {
			const QJsonObject o = v.toObject();
			m_announcementlist->addAnnouncement(o["url"].toString());
		}
	}

	if(config.contains("auth")) {
		m_authList->update(config["auth"].toArray());
	}
}

void Document::onAutoresetQueried(int maxSize, const QString &payload)
{
	Q_ASSERT(m_canvas);
	qDebug("Server queried autoreset");
	m_sessionHistoryMaxSize = maxSize;
	m_autoResetCorrelator.clear();

	if(shouldRespondToAutoReset()) {
		// Users can self-indicate in their network preferences that their
		// connection sucks and they don't want to be considered for autoresets.
		// This used to entirely disable autoresets, but of course that can
		// instead get you into a situation where the session runs out of space
		// or just takes forever to catch up to. The current behavior is for it
		// to communicate a low network quality to the server if it understands
		// that or just put in a big delay in responding to the autoreset
		// request, which should result in us not getting picked over others.
		bool serverAutoReset = m_settings.serverAutoReset();
		// Presence of a payload indicates that the server supports extended
		// autoreset capabilities introduced in Drawpile 2.2.2.
		if(!payload.isEmpty()) {
			QJsonArray pings;
			for(qint64 ping : m_pingHistory) {
				pings.append(qreal(ping) / 1000.0);
			}
			QJsonObject kwargs = {
				{QStringLiteral("payload"), payload},
				{QStringLiteral("capabilities"),
				 QJsonArray({QStringLiteral("gzip1")})},
				{QStringLiteral("os"), net::ServerCommand::autoresetOs()},
				{QStringLiteral("net"), serverAutoReset ? 0.0 : 100.0},
				{QStringLiteral("pings"), pings},
			};
			m_client->sendMessage(net::ServerCommand::make(
				QStringLiteral("ready-to-autoreset"), {}, kwargs));
		} else if(serverAutoReset) {
			m_client->sendMessage(
				net::ServerCommand::make(QStringLiteral("ready-to-autoreset")));
		} else {
			QTimer::singleShot(10000, Qt::VeryCoarseTimer, this, [this] {
				if(m_client->isConnected()) {
					m_client->sendMessage(net::ServerCommand::make(
						QStringLiteral("ready-to-autoreset")));
				}
			});
		}
	}
}

void Document::onAutoresetRequested(
	int maxSize, const QString &correlator, const QString &stream)
{
	Q_ASSERT(m_canvas);
	qDebug("Server requested autoreset");
	m_sessionHistoryMaxSize = maxSize;
	m_autoResetCorrelator = correlator;

	if(shouldRespondToAutoReset()) {
		if(stream == QStringLiteral("gzip1")) {
			// Streamed autoreset. Keep going as normal, send reset messages in
			// the background so the server can swap out its state on the fly.
			qDebug("Send stream-reset-start");
			m_client->sendMessage(net::ServerCommand::make(
				QStringLiteral("stream-reset-start"), {m_autoResetCorrelator}));
		} else {
			// Interrupting autoreset. Stop everything and send a reset image.
			// TODO: There seems to be a race condition in the server where
			// it will fail to actually send this command out to the clients
			// if the subsequent server command comes in too fast.
			sendLockSession(true);
			m_client->sendMessage(net::makeChatMessage(
				m_client->myId(), DP_MSG_CHAT_TFLAGS_BYPASS,
				DP_MSG_CHAT_OFLAGS_ACTION,
				QStringLiteral("beginning session autoreset...")));
			sendResetSession();
		}
	}
}

void Document::onStreamResetStarted(const QString &correlator)
{
	if(m_canvas && !correlator.isEmpty() &&
	   m_autoResetCorrelator == correlator) {
		net::Message msg =
			net::makeInternalStreamResetStartMessage(0, correlator);
		m_canvas->handleCommands(1, &msg);
	}
}

void Document::onStreamResetProgressed(bool cancel)
{
	switch(m_streamResetState) {
	case StreamResetState::Streaming:
		if(cancel) {
			qWarning("Stream reset cancelled while streaming it");
			setStreamResetState(StreamResetState::None);
		} else {
			sendNextStreamResetMessage();
		}
		break;
	case StreamResetState::Generating:
		if(cancel) {
			qWarning("Stream reset cancelled while generating the reset image");
			setStreamResetState(StreamResetState::None);
		} else {
			qWarning("Stream reset progress requested, but we're still "
					 "generating the reset image");
		}
		break;
	default:
		qWarning(
			"Stream reset %s requested, but none is active",
			cancel ? "cancel" : "progress");
		setStreamResetState(StreamResetState::None);
	}
}

void Document::buildStreamResetImage(
	const drawdance::CanvasState &canvasState, const QString &correlator)
{
	if(correlator == m_autoResetCorrelator) {
		setStreamResetState(StreamResetState::Generating);
		qDebug("Building stream reset image");
		net::MessageList metadata;
		int prepended = m_canvas->amendSnapshotMetadata(
			metadata, true, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS);
		utils::FunctionRunnable *runnable = new utils::FunctionRunnable(
			[this, canvasState, correlator, metadata, prepended]() {
				int messageCount;
				net::MessageList image = generateStreamSnapshot(
					canvasState, metadata, prepended, messageCount);
				emit buildStreamResetImageFinished(
					image, messageCount, correlator);
			});
		QThreadPool::globalInstance()->start(runnable);
	} else {
		qWarning("Stream reset start triggered, but correlator does not match");
	}
}

void Document::onMoveLayerRequested(
	int sourceId, int targetId, bool intoGroup, bool below)
{
	uint8_t contextId = m_client->myId();
	net::Message msg;
	if(m_client->isCompatibilityMode()) {
		msg = m_canvas->paintEngine()->historyCanvasState().makeLayerOrder(
			contextId, sourceId, targetId, below);
	} else {
		msg = m_canvas->paintEngine()->historyCanvasState().makeLayerTreeMove(
			contextId, sourceId, targetId, intoGroup, below);
	}
	if(msg.isNull()) {
		qWarning("Can't move layer: %s", DP_error());
	} else {
		net::Message messages[] = {net::makeUndoPointMessage(contextId), msg};
		m_client->sendMessages(DP_ARRAY_LENGTH(messages), messages);
	}
}

void Document::setSessionPersistent(bool p)
{
	if(m_sessionPersistent != p) {
		m_sessionPersistent = p;
		emit sessionPersistentChanged(p);
	}
}

void Document::setSessionClosed(bool closed)
{
	if(m_sessionClosed != closed) {
		m_sessionClosed = closed;
		emit sessionClosedChanged(closed);
	}
}

void Document::setSessionAuthOnly(bool authOnly)
{
	if(m_sessionAuthOnly != authOnly) {
		m_sessionAuthOnly = authOnly;
		emit sessionAuthOnlyChanged(authOnly);
	}
}

void Document::setSessionAllowWeb(bool allowWeb)
{
	// Always emit this, since we make showing the checkbox dependent on it.
	m_sessionAllowWeb = allowWeb;
	// The user needs to have the WEBSESSION flag to be allowed to change
	// this setting. If they're connected via WebSocket themselves, they
	// can't turn it off, since it would lock themselves out of rejoining.
	bool canAlterAllowWeb = m_client->canManageWebSession() &&
							(!m_client->isWebSocket() || !m_sessionAllowWeb);
	emit sessionAllowWebChanged(m_sessionAllowWeb, canAlterAllowWeb);
}

void Document::setSessionMaxUserCount(int count)
{
	// The server may cap this, so always emit this value.
	m_sessionMaxUserCount = count;
	emit sessionMaxUserCountChanged(count);
}

void Document::setSessionResetThreshold(int threshold)
{
	// Note: always emit TresholdChanged, since the server may cap the value
	// if a low hard size limit is in place. This ensures the settings dialog
	// value is always up to date.
	m_sessionResetThreshold = threshold;
	emit sessionResetThresholdChanged(threshold / double(1024 * 1024));
}

void Document::setBaseResetThreshold(int threshold)
{
	if(m_baseResetThreshold != threshold) {
		m_baseResetThreshold = threshold;
		emit baseResetThresholdChanged(threshold);
	}
}

void Document::setSessionPreserveChat(bool pc)
{
	if(m_sessionPreserveChat != pc) {
		m_sessionPreserveChat = pc;
		emit sessionPreserveChatChanged(pc);
	}
}

void Document::setSessionPasswordProtected(bool pp)
{
	if(m_sessionPasswordProtected != pp) {
		m_sessionPasswordProtected = pp;
		emit sessionPasswordChanged(pp);
	}
}

void Document::setSessionOpword(bool ow)
{
	if(m_sessionOpword != ow) {
		m_sessionOpword = ow;
		emit sessionOpwordChanged(ow);
	}
}

void Document::setSessionNsfm(bool nsfm)
{
	if(m_sessionNsfm != nsfm) {
		m_sessionNsfm = nsfm;
		emit sessionNsfmChanged(nsfm);
	}
}

void Document::setSessionForceNsfm(bool forceNsfm)
{
	if(m_sessionForceNsfm != forceNsfm) {
		m_sessionForceNsfm = forceNsfm;
		emit sessionForceNsfmChanged(forceNsfm);
	}
}

void Document::setSessionDeputies(bool deputies)
{
	if(m_sessionDeputies != deputies) {
		m_sessionDeputies = deputies;
		emit sessionDeputiesChanged(deputies);
	}
}

void Document::setSessionIdleTimeLimit(int idleTimeLimit)
{
	m_sessionIdleTimeLimit = idleTimeLimit;
}

void Document::setSessionIdleOverride(bool idleOverride)
{
	m_sessionIdleOverride = idleOverride;
}

void Document::setSessionAllowIdleOverride(bool allowIdleOverride)
{
	m_sessionAllowIdleOverride = allowIdleOverride;
}

void Document::setSessionOutOfSpace(bool outOfSpace)
{
	if(outOfSpace != m_sessionOutOfSpace) {
		m_sessionOutOfSpace = outOfSpace;
		emit sessionOutOfSpaceChanged(outOfSpace);
	}
}

void Document::setAutosave(bool autosave)
{
	if(autosave && !canAutosave()) {
		qWarning("Can't autosave");
		return;
	}

	if(m_autosave != autosave) {
		m_autosave = autosave;
		emit autosaveChanged(autosave);

		if(autosave && isDirty()) {
			this->autosave();
		}
	}
}

void Document::setCurrentPath(const QString &path, DP_SaveImageType type)
{
	if(m_currentPath != path || m_currentType != type) {
		m_currentPath = path;
		m_currentType = path.isEmpty() ? DP_SAVE_IMAGE_UNKNOWN : type;
		emit currentPathChanged(path);

		const bool couldAutosave = m_canAutosave;
		m_canAutosave = m_currentType != DP_SAVE_IMAGE_UNKNOWN;
		if(couldAutosave != m_canAutosave) {
			emit canAutosaveChanged(m_canAutosave);
		}

		if(!canAutosave()) {
			setAutosave(false);
		}
	}
}

void Document::markDirty()
{
	bool wasDirty = m_dirty;
	m_dirty = true;
	if(m_autosave)
		autosave();

	if(!wasDirty)
		emit dirtyCanvas(m_dirty);
}

void Document::unmarkDirty()
{
	if(m_dirty) {
		m_dirty = false;
		emit dirtyCanvas(m_dirty);
	}
}

void Document::setWantCanvasHistoryDump(bool wantCanvasHistoryDump)
{
	m_wantCanvasHistoryDump = wantCanvasHistoryDump;
	if(m_canvas) {
		m_canvas->paintEngine()->setWantCanvasHistoryDump(
			wantCanvasHistoryDump);
	}
}

QString Document::sessionTitle() const
{
	if(m_canvas)
		return m_canvas->title();
	else
		return QString();
}

void Document::clearCurrentPath()
{
	setCurrentPath(QString(), DP_SAVE_IMAGE_UNKNOWN);
}

QString Document::downloadName() const
{
	return (m_downloadName.isEmpty() ? sessionTitle() : m_downloadName)
		.trimmed();
}

void Document::setDownloadName(const QString &downloadName)
{
	m_downloadName = downloadName;
}

bool Document::isCompatibilityMode() const
{
	return m_client->isCompatibilityMode();
}

void Document::handleCommands(int count, const net::Message *msgs)
{
	if(m_canvas) {
		m_canvas->handleCommands(count, msgs);
	}
}

void Document::handleLocalCommands(int count, const net::Message *msgs)
{
	if(m_canvas) {
		m_canvas->handleLocalCommands(count, msgs);
	}
}

bool Document::checkPermission(int feature)
{
	return m_canvas && m_canvas->checkPermission(feature);
}

void Document::autosave()
{
	if(!m_autosaveTimer->isActive()) {
		int autosaveInterval =
			qMax(0, m_settings.autoSaveIntervalMinutes() * 60000);
		m_autosaveTimer->start(autosaveInterval);
	}
}

void Document::autosaveNow()
{
	if(!isDirty() || !isAutosave() || m_saveInProgress)
		return;

	saveCanvasState(
		m_canvas->paintEngine()->viewCanvasState(), true, currentPath(),
		currentType());
}

void Document::saveCanvasAs(
	const QString &filename, DP_SaveImageType type, bool exported)
{
	saveCanvasStateAs(
		filename, type, m_canvas->paintEngine()->viewCanvasState(), true,
		exported);
}

void Document::saveCanvasStateAs(
	const QString &path, DP_SaveImageType type,
	const drawdance::CanvasState &canvasState, bool isCurrentState,
	bool exported)
{
	if(!exported) {
		setCurrentPath(path, type);
	}
	saveCanvasState(canvasState, isCurrentState, path, type);
}

void Document::saveCanvasState(
	const drawdance::CanvasState &canvasState, bool isCurrentState,
	const QString &path, DP_SaveImageType type)
{
	Q_ASSERT(!m_saveInProgress);
	m_saveInProgress = true;

	CanvasSaverRunnable *saver = new CanvasSaverRunnable(
		canvasState, type, path, m_canvas ? m_canvas->paintEngine() : nullptr);
	if(isCurrentState) {
		unmarkDirty();
	}
	connect(
		saver, &CanvasSaverRunnable::saveComplete, this,
		&Document::onCanvasSaved);
	emit canvasSaveStarted();
	QThreadPool::globalInstance()->start(saver);
}

void Document::exportTemplate(const QString &path)
{
	net::MessageList snapshot = m_canvas->generateSnapshot(
		false, DP_ACL_STATE_RESET_IMAGE_TEMPLATE_FLAGS);
	drawdance::RecordStartResult result =
		m_canvas->paintEngine()->exportTemplate(path, snapshot);
	QString errorMessage;
	switch(result) {
	case drawdance::RECORD_START_SUCCESS:
		errorMessage = QString{};
		break;
	case drawdance::RECORD_START_UNKNOWN_FORMAT:
		errorMessage = tr("Unknown format.");
		break;
	case drawdance::RECORD_START_HEADER_ERROR:
		errorMessage = tr("Header error.");
		break;
	case drawdance::RECORD_START_OPEN_ERROR:
		errorMessage = tr("Error opening file.");
		break;
	case drawdance::RECORD_START_RECORDER_ERROR:
		errorMessage = tr("Error starting recorder.");
		break;
	default:
		errorMessage = tr("Unknown error.");
		break;
	}
	emit templateExported(errorMessage);
}

void Document::onCanvasSaved(const QString &errorMessage)
{
	m_saveInProgress = false;

	if(!errorMessage.isEmpty())
		markDirty();

	emit canvasSaved(errorMessage);
}

drawdance::RecordStartResult Document::startRecording(const QString &filename)
{
	// Set file suffix if missing
	const QFileInfo info(filename);
	if(info.suffix().isEmpty()) {
		m_originalRecordingFilename = filename + ".dprec";
	} else {
		m_originalRecordingFilename = filename;
	}
	return m_canvas->startRecording(m_originalRecordingFilename);
}

bool Document::stopRecording()
{
	return m_canvas->stopRecording();
}

bool Document::isRecording() const
{
	return m_canvas && m_canvas->isRecording();
}

void Document::sendPointerMove(const QPointF &point)
{
	m_client->sendMessage(net::makeMovePointerMessage(
		m_client->myId(), point.x() * 4, point.y() * 4));
}

void Document::sendSessionConf(const QJsonObject &sessionconf)
{
	m_client->sendMessage(
		net::ServerCommand::make("sessionconf", QJsonArray(), sessionconf));
}

void Document::sendFeatureAccessLevelChange(
	const uint8_t tiers[DP_FEATURE_COUNT])
{
	m_client->sendMessage(net::makeFeatureAccessLevelsMessage(
		m_client->myId(), DP_FEATURE_COUNT, tiers));
}

void Document::sendLockSession(bool lock)
{
	m_client->sendMessage(net::makeLayerAclMessage(
		m_client->myId(), 0, lock ? DP_ACL_ALL_LOCKED_BIT : 0, {}));
}

void Document::sendOpword(const QString &opword)
{
	m_client->sendMessage(
		net::ServerCommand::make("gain-op", QJsonArray() << opword));
}

/**
 * @brief Generate a reset snapshot and send a reset request
 *
 * The reset image will be sent when the server acknowledges the request.
 * If an empty reset image is given here, one will be generated just in time.
 *
 * If the document is in offline mode, this will immediately reset the current
 * canvas.
 */
void Document::sendResetSession(const net::MessageList &resetImage)
{
	if(!m_client->isConnected()) {
		if(resetImage.isEmpty()) {
			qWarning("Tried to do an offline session reset with a blank reset "
					 "image");
			return;
		}
		// Not connected? Do a local reset
		initCanvas();
		m_client->sendResetMessages(resetImage.count(), resetImage.data());
		return;
	}

	if(resetImage.isEmpty()) {
		qInfo("Sending session reset request. (Just in time snapshot)");
	} else {
		qInfo(
			"Sending session reset request. (Snapshot size is %lld bytes)",
			compat::cast<long long>(resetImage.length()));
	}

	m_resetstate = resetImage;
	m_client->sendMessage(net::ServerCommand::make("reset-session"));
}

void Document::sendResizeCanvas(int top, int right, int bottom, int left)
{
	if(checkPermission(DP_FEATURE_RESIZE)) {
		uint8_t contextId = m_client->myId();
		net::Message msgs[] = {
			net::makeUndoPointMessage(contextId),
			net::makeCanvasResizeMessage(contextId, top, right, bottom, left),
		};
		m_client->sendMessages(DP_ARRAY_LENGTH(msgs), msgs);
	}
}

void Document::sendUnban(int entryId)
{
	m_client->sendMessage(net::ServerCommand::makeUnban(entryId));
}

void Document::sendAnnounce(const QString &url)
{
	m_client->sendMessage(net::ServerCommand::makeAnnounce(url));
}

void Document::sendUnannounce(const QString &url)
{
	m_client->sendMessage(net::ServerCommand::makeUnannounce(url));
}

void Document::sendTerminateSession(const QString &reason)
{
	QJsonObject kwargs;
	QString trimmed = reason.trimmed().mid(0, 255);
	if(!trimmed.isEmpty()) {
		kwargs[QStringLiteral("reason")] = trimmed;
	}
	m_client->sendMessage(net::ServerCommand::make("kill-session", {}, kwargs));
}

void Document::sendCanvasBackground(const QColor &color)
{
	if(checkPermission(DP_FEATURE_BACKGROUND)) {
		uint8_t contextId = m_client->myId();
		net::Message msgs[] = {
			net::makeUndoPointMessage(contextId),
			net::makeCanvasBackgroundMessage(contextId, color),
		};
		m_client->sendMessages(DP_ARRAY_LENGTH(msgs), msgs);
	}
}

void Document::sendAbuseReport(int userId, const QString &message)
{
	QJsonObject kwargs;
	if(userId > 0 && userId < 256)
		kwargs["user"] = userId;
	kwargs["reason"] = message;
	m_client->sendMessage(
		net::ServerCommand::make("report", QJsonArray(), kwargs));
}

void Document::snapshotNeeded()
{
	// (We) requested a session reset and the server is now ready for it.
	if(m_canvas) {
		if(m_resetstate.isEmpty()) {
			// FunctionRunnable has autoDelete enabled
			auto runnable = new utils::FunctionRunnable([this]() {
				generateJustInTimeSnapshot();
			});
			QThreadPool::globalInstance()->start(runnable);
		} else {
			sendResetSnapshot();
		}
	} else {
		qWarning(
			"Server requested snapshot, but canvas is not yet initialized!");
		m_client->sendMessage(net::ServerCommand::make("init-cancel"));
	}
}

bool Document::shouldRespondToAutoReset() const
{
	if(!m_client->isFullyCaughtUp()) {
		qInfo("Ignoring autoreset request because we're not caught up yet");
		return false;
	} else {
		return true;
	}
}

void Document::generateJustInTimeSnapshot()
{
	qInfo("Generating a just-in-time snapshot for session reset...");
	m_resetstate = m_canvas->generateSnapshot(
		true, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS);
	if(m_resetstate.isEmpty()) {
		qWarning("Just-in-time snapshot has zero size!");
		m_client->sendMessage(net::ServerCommand::make("init-cancel"));
	} else {
		emit justInTimeSnapshotGenerated();
	}
}

void Document::sendResetSnapshot()
{
	// Size limit check. The server will kick us if we send an oversized reset.
	if(m_sessionHistoryMaxSize > 0 &&
	   m_resetstate.length() > m_sessionHistoryMaxSize) {
		qWarning(
			"Reset snapshot (%lld) is larger than the size limit (%d)!",
			compat::cast<long long>(m_resetstate.length()),
			m_sessionHistoryMaxSize);
		emit autoResetTooLarge(m_sessionHistoryMaxSize);
		m_client->sendMessage(net::ServerCommand::make("init-cancel"));
	} else {
		// Send the reset command+image
		m_client->sendResetMessage(net::ServerCommand::make("init-begin"));
		m_client->sendResetMessages(
			m_resetstate.count(), m_resetstate.constData());
		m_client->sendResetMessage(net::ServerCommand::make("init-complete"));
	}
	m_resetstate.clear();
}

namespace {
struct ResetStreamImageContext {
	DP_ResetStreamProducer *rsp;
	int count;
	bool ok;

	static void push(void *user, DP_Message *msg)
	{
		ResetStreamImageContext *ctx =
			static_cast<ResetStreamImageContext *>(user);
		if(ctx->ok) {
			if(DP_reset_stream_producer_push(ctx->rsp, msg)) {
				++ctx->count;
			} else {
				qWarning("Reset stream: %s", DP_error());
				ctx->ok = false;
			}
		}
		DP_message_decref(msg);
	}
};
}

net::MessageList Document::generateStreamSnapshot(
	const drawdance::CanvasState &canvasState, const net::MessageList &metadata,
	int prepended, int &outMessageCount) const
{
	DP_ResetStreamProducer *rsp = DP_reset_stream_producer_new();
	if(!rsp) {
		qWarning("Error initializing reset stream producer: %s", DP_error());
		return {};
	}

	for(int i = 0; i < prepended; ++i) {
		if(!DP_reset_stream_producer_push(rsp, metadata[i].get())) {
			qWarning("Reset stream: %s", DP_error());
			DP_reset_stream_producer_free_discard(rsp);
			return {};
		}
	}

	ResetStreamImageContext ctx = {rsp, 0, true};
	DP_reset_image_build(
		canvasState.get(), 0, ResetStreamImageContext::push, &ctx);
	if(!ctx.ok) {
		DP_reset_stream_producer_free_discard(rsp);
		return {};
	}

	int metadataCount = metadata.size();
	for(int i = prepended; i < metadataCount; ++i) {
		if(!DP_reset_stream_producer_push(rsp, metadata[i].get())) {
			qWarning("Reset stream: %s", DP_error());
			DP_reset_stream_producer_free_discard(rsp);
			return {};
		}
	}

	if(m_sessionHistoryMaxSize > 0 && DP_reset_stream_producer_image_size(rsp) >
										  size_t(m_sessionHistoryMaxSize)) {
		qWarning("Reset stream: oversized reset image");
		DP_reset_stream_producer_free_discard(rsp);
		return {};
	}

	int count;
	DP_Message **msgs = DP_reset_stream_producer_free_finish(rsp, &count);
	if(!msgs) {
		qWarning("Reset stream: %s", DP_error());
		return {};
	}

	net::MessageList image;
	image.reserve(count);
	for(int i = 0; i < count; ++i) {
		image.append(net::Message::noinc(msgs[i]));
	}
	DP_free(msgs);

	outMessageCount = metadataCount + ctx.count;
	return image;
}

void Document::startSendingStreamResetSnapshot(
	const QVector<net::Message> &image, int messageCount,
	const QString &correlator)
{
	if(m_streamResetState == StreamResetState::Generating &&
	   m_autoResetCorrelator == correlator) {
		if(messageCount > 0 && !image.isEmpty()) {
			qDebug("Start sending stream reset image");
			m_streamResetImage = image;
			m_streamResetImageOriginalCount = image.size();
			setStreamResetState(StreamResetState::Streaming, messageCount);
			sendNextStreamResetMessage();
		} else {
			qDebug("Abort sending stream reset image");
			m_streamResetImage.clear();
			m_streamResetImageOriginalCount = 0;
			setStreamResetState(StreamResetState::None);
			m_client->sendMessage(
				net::ServerCommand::make(QStringLiteral("stream-reset-abort")));
		}
	} else {
		qWarning("Streamed reset image generated, but not ready to stream");
	}
}

void Document::sendNextStreamResetMessage()
{
	if(m_streamResetState == StreamResetState::Streaming) {
		if(m_streamResetImage.isEmpty()) {
			qDebug("Send stream-reset-finish");
			m_client->sendMessage(net::ServerCommand::make(
				QStringLiteral("stream-reset-finish"),
				{m_streamResetMessageCount}));
			setStreamResetState(StreamResetState::None);
		} else {
			m_client->sendMessage(m_streamResetImage.takeFirst());
			emitStreamResetProgress();
		}
	} else {
		setStreamResetState(StreamResetState::None);
	}
}

void Document::setStreamResetState(StreamResetState state, int messageCount)
{
	m_streamResetState = state;
	m_streamResetMessageCount = messageCount;
	emitStreamResetProgress();
}

void Document::emitStreamResetProgress()
{
	switch(m_streamResetState) {
	case StreamResetState::None:
		emit streamResetProgress(101);
		break;
	case StreamResetState::Generating:
		emit streamResetProgress(-1);
		break;
	case StreamResetState::Streaming: {
		qreal total = m_streamResetImageOriginalCount;
		qreal sent = total - m_streamResetImage.size();
		emit streamResetProgress(qBound(0, qRound(sent / total * 100.0), 100));
		break;
	}
	}
}

void Document::undo()
{
	if(!m_canvas)
		return;
	// Only allow undos if the pen or mouse button is not down. If we're in a
	// multipart drawing, e.g. with the bezier tool, we only undo the last part.
	if(!m_toolctrl->isDrawing() && !m_toolctrl->undoMultipartDrawing() &&
	   checkPermission(DP_FEATURE_UNDO)) {
		m_client->sendMessage(net::makeUndoMessage(m_client->myId(), 0, false));
	}
}

void Document::redo()
{
	if(!m_canvas)
		return;
	// Only allow undos if the pen or mouse button is not down. If we're in a
	// multipart drawing, e.g. with a transform, we only redo the last part.
	if(!m_toolctrl->isDrawing() && !m_toolctrl->redoMultipartDrawing() &&
	   checkPermission(DP_FEATURE_UNDO)) {
		m_client->sendMessage(net::makeUndoMessage(m_client->myId(), 0, true));
	}
}

void Document::selectAll()
{
	selectAllOp(DP_MSG_SELECTION_PUT_OP_REPLACE);
}

void Document::selectNone()
{
	if(!m_toolctrl->transformTool()->handleDeselect() && m_canvas &&
	   m_canvas->selection()->isValid()) {
		unsigned int contextId = m_client->myId();
		net::Message msgs[] = {
			net::makeUndoPointMessage(contextId),
			net::makeSelectionClearMessage(
				contextId, canvas::CanvasModel::MAIN_SELECTION_ID),
		};
		m_client->sendMessages(2, msgs);
	}
}

void Document::selectInvert()
{
	selectAllOp(DP_MSG_SELECTION_PUT_OP_COMPLEMENT);
}

void Document::selectLayerBounds()
{
	selectLayer(false);
	if(m_canvas) {
		int layerId = m_toolctrl->activeLayer();
		QRect bounds =
			m_canvas->paintEngine()->viewCanvasState().layerBounds(layerId);
		if(bounds.isEmpty()) {
			selectNone();
		} else {
			selectOp(DP_MSG_SELECTION_PUT_OP_REPLACE, bounds);
		}
	}
}

void Document::selectLayerContents()
{
	selectLayer(true);
}

void Document::selectMask(const QImage &img, int x, int y)
{
	if(img.isNull()) {
		selectNone();
	} else {
		selectOp(
			DP_MSG_SELECTION_PUT_OP_REPLACE,
			QRect(x, y, img.width(), img.height()), img);
	}
}

void Document::selectLayer(bool includeMask)
{
	if(m_canvas) {
		int layerId = m_toolctrl->activeLayer();
		drawdance::CanvasState canvasState =
			m_canvas->paintEngine()->viewCanvasState();
		QRect bounds = canvasState.layerBounds(layerId);
		if(bounds.isEmpty()) {
			selectNone();
		} else {
			selectOp(
				DP_MSG_SELECTION_PUT_OP_REPLACE, bounds,
				includeMask ? canvasState.layerToFlatImage(layerId, bounds)
							: QImage());
		}
	}
}

void Document::selectAllOp(int op)
{
	if(m_canvas) {
		QSize size = m_canvas->size();
		if(!size.isEmpty()) {
			selectOp(op, QRect(QPoint(0, 0), size));
		}
	}
}

void Document::selectOp(int op, const QRect &bounds, const QImage &mask)
{
	unsigned int contextId = m_client->myId();
	net::MessageList msgs;
	net::makeSelectionPutMessages(
		msgs, contextId, canvas::CanvasModel::MAIN_SELECTION_ID, op, bounds.x(),
		bounds.y(), bounds.width(), bounds.height(), mask);
	if(!msgs.isEmpty()) {
		msgs.prepend(net::makeUndoPointMessage(contextId));
		m_client->sendMessages(msgs.size(), msgs.constData());
	}
}

bool Document::copyFromLayer(int layer)
{
	if(!m_canvas || !m_canvas->selection()->isValid() ||
	   m_canvas->transform()->isActive()) {
		return false;
	}

#ifdef HAVE_CLIPBOARD_EMULATION
	QMimeData *data = &clipboardData;
	data->clear();
#else
	QMimeData *data = new QMimeData;
#endif

	bool found;
	QImage img = m_canvas->selectionToImage(layer, &found);
	if(!img.isNull() && layer == 0) {
		fillBackground(img);
	}

#ifdef Q_OS_LINUX
	// Copying image data from one Drawpile canvas to another is busted on
	// Wayland, obliterating transparency. We store a PNG directly instead.
	if(QGuiApplication::platformName() == QStringLiteral("wayland")) {
		QByteArray bytes;
		{
			QBuffer buffer(&bytes);
			buffer.open(QIODevice::WriteOnly);
			img.save(&buffer, "PNG");
		}
		data->setData(QStringLiteral("image/png"), bytes);
	} else {
		data->setImageData(img);
	}
#else
	data->setImageData(img);
#endif

	// Store also original coordinates
	QRect bounds = m_canvas->selection()->bounds();
	QPoint srcpos =
		bounds.topLeft() + QPoint(img.width() / 2, img.height() / 2);
	QByteArray srcbuf = QByteArray::number(srcpos.x()) + "," +
						QByteArray::number(srcpos.y()) + "," +
						QByteArray::number(qApp->applicationPid()) + "," +
						QByteArray::number(pasteId());
	data->setData("x-drawpile/pastesrc", srcbuf);

#ifndef HAVE_CLIPBOARD_EMULATION
	QGuiApplication::clipboard()->setMimeData(data);
#endif

	return found;
}

bool Document::saveSelection(const QString &path)
{
	QImage img = m_canvas->selectionToImage(0);
	if(img.isNull()) {
		return false;
	} else {
		fillBackground(img);
		return img.save(path);
	}
}

void Document::fillBackground(QImage &img)
{
	// Fill transparent pixels (i.e. area outside the selection)
	// with the canvas background color.
	// TODO background pattern support
	QColor bg = m_canvas->paintEngine()->viewBackgroundColor();
	if(bg.isValid()) {
		QPainter p(&img);
		p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		p.fillRect(0, 0, img.width(), img.height(), bg);
	}
}

#ifdef __EMSCRIPTEN__
void Document::downloadCanvas(
	const QString &fileName, DP_SaveImageType type, QTemporaryDir *tempDir)
{
	downloadCanvasState(
		fileName, type, tempDir, m_canvas->paintEngine()->viewCanvasState());
}

void Document::downloadCanvasState(
	const QString &fileName, DP_SaveImageType type, QTemporaryDir *tempDir,
	const drawdance::CanvasState &canvasState)
{
	QString path = tempDir->filePath(fileName);
	CanvasSaverRunnable *saver = new CanvasSaverRunnable(
		canvasState, type, path, m_canvas ? m_canvas->paintEngine() : nullptr,
		tempDir);
	saver->setAutoDelete(false);
	connect(
		saver, &CanvasSaverRunnable::saveComplete, this,
		[this, saver, fileName, path](const QString &error) {
			if(error.isEmpty()) {
				QFile file(path);
				file.open(QIODevice::ReadOnly);
				emit canvasDownloadReady(fileName, file.readAll());
			} else {
				emit canvasDownloadError(error);
			}
			saver->deleteLater();
		});
	emit canvasDownloadStarted();
	QThreadPool::globalInstance()->start(saver);
}

void Document::downloadSelection(const QString &fileName)
{
	emit canvasDownloadStarted();
	QFileInfo fileInfo(fileName);
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(DP_QT_WRITE_FLAGS);
	bool ok = selectionToImage().save(
		&buffer, qUtf8Printable(fileInfo.completeSuffix()));
	buffer.close();
	if(ok) {
		emit canvasDownloadReady(fileName, bytes);
	} else {
		emit canvasDownloadError(tr("Error saving image"));
	}
}
#endif

QImage Document::selectionToImage()
{
	QImage img = m_canvas->selectionToImage(0);
	fillBackground(img);
	return img;
}

void Document::copyVisible()
{
	copyFromLayer(0);
}

void Document::copyMerged()
{
	copyFromLayer(-1);
}


void Document::copyLayer()
{
	copyFromLayer(m_toolctrl->activeLayer());
}

void Document::cutLayer()
{
	if(checkPermission(DP_FEATURE_PUT_IMAGE) &&
	   copyFromLayer(m_toolctrl->activeLayer())) {
		clearArea();
	}
}

void Document::clearArea()
{
	if(checkPermission(DP_FEATURE_PUT_IMAGE)) {
		fillArea(Qt::white, DP_BLEND_MODE_ERASE, 1.0f);
	}
}

void Document::fillArea(const QColor &color, DP_BlendMode mode, float opacity)
{
	if(!m_canvas || opacity <= 0.0f) {
		return;
	}

	canvas::SelectionModel *selection = m_canvas->selection();
	if(!selection->isValid()) {
		return;
	}

	QSet<int> layerIds =
		m_canvas->layerlist()->getModifiableLayers(m_toolctrl->activeLayer());
	if(layerIds.isEmpty()) {
		return;
	}

	QImage mask = selection->mask();
	if(mask.isNull() || mask.size().isEmpty()) {
		return;
	}

	if(!checkPermission(DP_FEATURE_PUT_IMAGE)) {
		return;
	}

	{
		QPainter painter(&mask);
		painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
		QRect maskRect = mask.rect();
		painter.fillRect(
			maskRect, QColor(color.red(), color.green(), color.blue()));
		if(opacity < 1.0f) {
			painter.setCompositionMode(
				QPainter::CompositionMode_DestinationOut);
			painter.fillRect(
				maskRect,
				QColor::fromRgbF(0.0f, 0.0f, 0.0f, 1.0f - qMin(opacity, 1.0f)));
		}
	}

	uint8_t contextId = m_client->myId();
	QPoint pos = selection->bounds().topLeft();
	for(int layerId : layerIds) {
		net::makePutImageMessages(
			m_messageBuffer, contextId, layerId, mode, pos.x(), pos.y(), mask);
	}
	if(m_messageBuffer.isEmpty()) {
		return;
	}

	m_messageBuffer.prepend(net::makeUndoPointMessage(contextId));
	m_client->sendMessages(m_messageBuffer.size(), m_messageBuffer.constData());
	m_messageBuffer.clear();
}

void Document::deleteAnnotation(int annotationId)
{
	unsigned int contextId = m_client->myId();
	net::Message msgs[] = {
		net::makeUndoPointMessage(contextId),
		net::makeAnnotationDeleteMessage(contextId, annotationId),
	};
	m_client->sendMessages(2, msgs);
}

void Document::removeEmptyAnnotations()
{
	if(!m_canvas) {
		qWarning("removeEmptyAnnotations(): no canvas");
		return;
	}

	uint8_t contextId = m_canvas->localUserId();
	drawdance::AnnotationList al =
		m_canvas->paintEngine()->historyCanvasState().annotations();
	int count = al.count();
	for(int i = 0; i < count; ++i) {
		drawdance::Annotation a = al.at(i);
		if(a.textBytes().length() == 0) {
			m_messageBuffer.append(
				net::makeAnnotationDeleteMessage(contextId, a.id()));
		}
	}

	if(!m_messageBuffer.isEmpty()) {
		m_messageBuffer.prepend(net::makeUndoPointMessage(contextId));
		m_client->sendMessages(
			m_messageBuffer.size(), m_messageBuffer.constData());
	}
	m_messageBuffer.clear();
}

void Document::addServerLogEntry(const QString &log)
{
	int i = m_serverLog->rowCount();
	m_serverLog->insertRow(i);
	m_serverLog->setData(m_serverLog->index(i), log);
}

const QMimeData *Document::getClipboardData()
{
#ifdef HAVE_CLIPBOARD_EMULATION
	return &clipboardData;
#else
	return QGuiApplication::clipboard()->mimeData();
#endif
}

QImage Document::getClipboardImageData(const QMimeData *mimeData)
{
	if(mimeData) {
		if(mimeData->hasImage()) {
			return mimeData->imageData().value<QImage>();
		} else if(mimeData->hasFormat(QStringLiteral("image/png"))) {
			QImage img;
			if(img.loadFromData(
				   mimeData->data(QStringLiteral("image/png")), "PNG")) {
				return img;
			}
		}
	}
	return QImage();
}

#ifdef HAVE_CLIPBOARD_EMULATION
QMimeData Document::clipboardData;
#endif
