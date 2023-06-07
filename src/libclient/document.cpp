// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/document.h"

#include "libclient/net/client.h"
#include "libclient/net/servercmd.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selection.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/userlist.h"
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/settings.h"
#include "libclient/tools/selection.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/images.h"
#include "libshared/util/functionrunnable.h"
#include "libshared/util/qtcompat.h"

#include <QGuiApplication>
#include <QTimer>
#include <QDir>
#include <QClipboard>
#include <QThreadPool>
#include <QPainter>
#include <QtEndian>
#include <QRunnable>
#include <QSysInfo>
#include <tuple>
#include <parson.h>

Document::Document(libclient::settings::Settings &settings, QObject *parent)
	: QObject(parent),
	  m_resetstate(),
	  m_messageBuffer(),
	  m_canvas(nullptr),
	  m_settings(settings),
	  m_dirty(false),
	  m_autosave(false),
	  m_canAutosave(false),
	  m_saveInProgress(false),
	  m_wantCanvasHistoryDump(false),
	  m_sessionPersistent(false),
	  m_sessionClosed(false),
	  m_sessionAuthOnly(false),
	  m_sessionPreserveChat(false),
	  m_sessionPasswordProtected(false),
	  m_sessionOpword(false),
	  m_sessionNsfm(false),
	  m_sessionForceNsfm(false),
	  m_sessionDeputies(false),
	  m_sessionMaxUserCount(0),
	  m_sessionHistoryMaxSize(0),
	  m_sessionResetThreshold(0),
	  m_baseResetThreshold(0)
{
	// Initialize
	m_client = new net::Client(this);
	m_toolctrl = new tools::ToolController(m_client, this);
	m_settings.bindSmoothing(m_toolctrl, &tools::ToolController::setSmoothing);
	m_banlist = new net::BanlistModel(this);
	m_announcementlist = new net::AnnouncementListModel(settings.listServers(), this);
	m_serverLog = new QStringListModel(this);

	m_autosaveTimer = new QTimer(this);
	m_autosaveTimer->setSingleShot(true);
	connect(m_autosaveTimer, &QTimer::timeout, this, &Document::autosaveNow);

	// Make connections
	connect(m_client, &net::Client::serverConnected, this, &Document::serverConnected);
	connect(m_client, &net::Client::serverLoggedIn, this, &Document::onServerLogin);
	connect(m_client, &net::Client::serverDisconnected, this, &Document::onServerDisconnect);
	connect(m_client, &net::Client::serverDisconnected, this, &Document::serverDisconnected);

	connect(m_client, &net::Client::needSnapshot, this, &Document::snapshotNeeded);
	connect(m_client, &net::Client::sessionConfChange, this, &Document::onSessionConfChanged);
	connect(m_client, &net::Client::autoresetRequested, this, &Document::onAutoresetRequested);
	connect(m_client, &net::Client::serverLog, this, &Document::addServerLogEntry);

	connect(m_client, &net::Client::sessionResetted, this, &Document::onSessionResetted);

	connect(this, &Document::justInTimeSnapshotGenerated, this, &Document::sendResetSnapshot, Qt::QueuedConnection);
}

void Document::initCanvas()
{
	delete m_canvas;

	m_canvas = new canvas::CanvasModel{
		m_client->myId(),
		m_settings.engineFrameRate(),
		m_settings.engineSnapshotCount(),
		m_settings.engineSnapshotInterval() * 1000LL,
		m_wantCanvasHistoryDump, this
	};

	m_toolctrl->setModel(m_canvas);

	connect(m_client, &net::Client::messagesReceived, m_canvas, &canvas::CanvasModel::handleCommands);
	connect(m_client, &net::Client::drawingCommandsLocal, m_canvas, &canvas::CanvasModel::handleLocalCommands);
	connect(m_canvas, &canvas::CanvasModel::canvasModified, this, &Document::markDirty);
	connect(m_canvas->layerlist(), &canvas::LayerListModel::moveRequested, this, &Document::onMoveLayerRequested);

	connect(m_canvas, &canvas::CanvasModel::titleChanged, this, &Document::sessionTitleChanged);
	connect(m_canvas, &canvas::CanvasModel::recorderStateChanged, this, &Document::recorderStateChanged);

	connect(m_client, &net::Client::catchupProgress, m_canvas->paintEngine(), &canvas::PaintEngine::enqueueCatchupProgress);
	connect(m_canvas->paintEngine(), &canvas::PaintEngine::caughtUpTo, this, &Document::catchupProgress, Qt::QueuedConnection);

	emit canvasChanged(m_canvas);
	m_canvas->paintEngine()->resetAcl(m_client->myId());

	setCurrentFilename(QString());
}

void Document::onSessionResetted()
{
	Q_ASSERT(m_canvas);
	if(!m_canvas) {
		qWarning("sessionResetted: no canvas!");
		return;
	}

	// Cancel any possibly active local drawing process.
	// This prevents jumping lines across the canvas if it shifts.
	m_toolctrl->cancelMultipartDrawing();
	m_toolctrl->endDrawing();

	emit sessionResetState(m_canvas->paintEngine()->viewCanvasState());

	// Clear out the canvas in preparation for the new data that is about to follow
	m_canvas->resetCanvas();
	m_resetstate.clear();
}

bool Document::loadBlank(const QSize &size, const QColor &background)
{
	setAutosave(false);
	initCanvas();
	unmarkDirty();

	m_canvas->loadBlank(m_settings.engineUndoDepth(), size, background);
	setCurrentFilename(QString());
	return true;
}

DP_LoadResult Document::loadFile(const QString &path)
{
	DP_LoadResult result;
	drawdance::CanvasState canvasState = drawdance::CanvasState::load(path, &result);
	if(canvasState.isNull()) {
		Q_ASSERT(result != DP_LOAD_RESULT_SUCCESS);
		return result;
	} else {
		setAutosave(false);
		initCanvas();
		unmarkDirty();
		m_canvas->loadCanvasState(m_settings.engineUndoDepth(), canvasState);
		setCurrentFilename(path);
		return DP_LOAD_RESULT_SUCCESS;
	}
}

static bool isSessionTemplate(DP_Player *player)
{
	JSON_Object *header = DP_player_header(player);
	return header && DP_str_equal(json_object_get_string(header, "type"), "template");
}

DP_LoadResult Document::loadRecording(
	const QString &path, bool debugDump, bool *outIsTemplate)
{
	DP_LoadResult result;
	DP_Player *player = debugDump
		? DP_load_debug_dump(path.toUtf8().constData(), &result)
		: DP_load_recording(path.toUtf8().constData(), &result);
	bool isTemplate;
	switch (result) {
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
		setCurrentFilename(path);
		break;
	default:
		Q_ASSERT(!player);
		isTemplate = false;
		break;
	}
	if(outIsTemplate){
		*outIsTemplate = isTemplate;
	}
	return result;
}

void Document::onServerLogin(bool join, bool compatibilityMode)
{
	if(join)
		initCanvas();

	Q_ASSERT(m_canvas);

	m_canvas->connectedToServer(m_client->myId(), join, compatibilityMode);

	if(!m_recordOnConnect.isEmpty()) {
		m_originalRecordingFilename = m_recordOnConnect;
		startRecording(m_recordOnConnect);
		m_recordOnConnect = QString();
	}

	m_sessionHistoryMaxSize = 0;
	m_baseResetThreshold = 0;
	emit serverLoggedIn(join);
	emit compatibilityModeChanged(compatibilityMode);
}

void Document::onServerDisconnect()
{
	if(m_canvas) {
		m_canvas->disconnectedFromServer();
		m_canvas->setTitle(QString());
	}
	m_banlist->clear();
	m_announcementlist->clear();
	setSessionOpword(false);
	emit compatibilityModeChanged(false);
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
			const net::Announcement a {
				o["url"].toString(),
				o["roomcode"].toString(),
				o["private"].toBool()
			};
			m_announcementlist->addAnnouncement(a);
			if(!a.roomcode.isEmpty())
				jc = a.roomcode;
		}
		setRoomcode(jc);
	}
}

void Document::onAutoresetRequested(int maxSize, bool query)
{
	Q_ASSERT(m_canvas);

	qInfo("Server requested autoreset (query=%d)", query);

	if(!m_client->isFullyCaughtUp()) {
		qInfo("Ignoring autoreset request because we're not fully caught up yet.");
		return;
	}

	m_sessionHistoryMaxSize = maxSize;

	if(m_settings.serverAutoReset()) {
		if(query) {
			// This is just a query: send back an affirmative response
			m_client->sendMessage(net::ServerCommand::make("ready-to-autoreset"));

		} else {
			// Autoreset on request
			// TODO: There seems to be a race condition in the server where it
			// will fail to actually send this command out to the clients if the
			// subsequent server command comes in too fast.
			sendLockSession(true);

			m_client->sendMessage(drawdance::Message::makeChat(
				m_client->myId(), DP_MSG_CHAT_TFLAGS_BYPASS, DP_MSG_CHAT_OFLAGS_ACTION,
				QStringLiteral("beginning session autoreset...")));

			sendResetSession();
		}
	} else {
		qInfo("Ignoring autoreset request as configured.");
	}
}

void Document::onMoveLayerRequested(int sourceId, int targetId, bool intoGroup, bool below)
{
	uint8_t contextId = m_client->myId();
	drawdance::Message msg;
	if(m_client->isCompatibilityMode()) {
		msg = m_canvas->paintEngine()->historyCanvasState()
			.makeLayerOrder(contextId, sourceId, targetId, below);
	} else {
		msg = m_canvas->paintEngine()->historyCanvasState()
			.makeLayerTreeMove(contextId, sourceId, targetId, intoGroup, below);
	}
	if(msg.isNull()) {
		qWarning("Can't move layer: %s", DP_error());
	} else {
		drawdance::Message messages[] = {
			drawdance::Message::makeUndoPoint(contextId), msg};
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

void Document::setSessionMaxUserCount(int count)
{
	if(m_sessionMaxUserCount != count) {
		m_sessionMaxUserCount = count;
		emit sessionMaxUserCountChanged(count);
	}
}

void Document::setSessionResetThreshold(int threshold)
{
	// Note: always emit TresholdChanged, since the server may cap the value
	// if a low hard size limit is in place. This ensures the settings dialog
	// value is always up to date.
	m_sessionResetThreshold = threshold;
	emit sessionResetThresholdChanged(threshold / double(1024*1024));
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

void Document::setRoomcode(const QString &roomcode)
{
	if(m_roomcode != roomcode) {
		m_roomcode = roomcode;
		emit sessionRoomcodeChanged(roomcode);
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

void Document::setCurrentFilename(const QString &filename)
{
	if(m_currentFilename != filename) {
		m_currentFilename = filename;
		emit currentFilenameChanged(filename);

		const bool couldAutosave = m_canAutosave;
		m_canAutosave = utils::isWritableFormat(m_currentFilename);
		if(couldAutosave != m_canAutosave)
			emit canAutosaveChanged(m_canAutosave);

		if(!canAutosave())
			setAutosave(false);
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
		m_canvas->paintEngine()->setWantCanvasHistoryDump(wantCanvasHistoryDump);
	}
}

QString Document::sessionTitle() const
{
	if(m_canvas)
		return m_canvas->title();
	else
		return QString();
}

bool Document::isCompatibilityMode() const
{
	return m_client->isCompatibilityMode();
}

void Document::autosave()
{
	if(!m_autosaveTimer->isActive()) {
		const int autosaveInterval = qMax(0, m_settings.autoSaveInterval());
		m_autosaveTimer->start(autosaveInterval);
	}
}

void Document::autosaveNow()
{
	if(!isDirty() || !isAutosave() || m_saveInProgress)
		return;

	Q_ASSERT(utils::isWritableFormat(currentFilename()));

	saveCanvasState(m_canvas->paintEngine()->viewCanvasState(), true);
}

void Document::saveCanvasAs(const QString &filename)
{
	saveCanvasStateAs(filename, m_canvas->paintEngine()->viewCanvasState(), true);
}

void Document::saveCanvasStateAs(
	const QString &filename, const drawdance::CanvasState &canvasState,
	bool isCurrentState)
{
	setCurrentFilename(filename);
	saveCanvasState(canvasState, isCurrentState);
}

void Document::saveCanvasState(const drawdance::CanvasState &canvasState, bool isCurrentState)
{
	Q_ASSERT(!m_saveInProgress);
	m_saveInProgress = true;

	CanvasSaverRunnable *saver = new CanvasSaverRunnable(canvasState, m_currentFilename);
	if(isCurrentState) {
		unmarkDirty();
	}
	connect(saver, &CanvasSaverRunnable::saveComplete, this, &Document::onCanvasSaved);
	emit canvasSaveStarted();
	QThreadPool::globalInstance()->start(saver);
}

void Document::exportTemplate(const QString &path)
{
	drawdance::MessageList snapshot = m_canvas->generateSnapshot(
		false, DP_ACL_STATE_RESET_IMAGE_TEMPLATE_FLAGS);
	drawdance::RecordStartResult result = m_canvas->paintEngine()->exportTemplate(path, snapshot);
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
	m_client->sendMessage(drawdance::Message::makeMovePointer(
		m_client->myId(), point.x() * 4, point.y() * 4));
}

void Document::sendSessionConf(const QJsonObject &sessionconf)
{
	m_client->sendMessage(net::ServerCommand::make("sessionconf", QJsonArray(), sessionconf));
}

void Document::sendFeatureAccessLevelChange(const uint8_t tiers[DP_FEATURE_COUNT])
{
	m_client->sendMessage(drawdance::Message::makeFeatureAccessLevels(
		m_client->myId(), DP_FEATURE_COUNT, tiers));
}

void Document::sendLockSession(bool lock)
{
	m_client->sendMessage(drawdance::Message::makeLayerAcl(
		m_client->myId(), 0, lock ? DP_ACL_ALL_LOCKED_BIT : 0, {}));
}

void Document::sendOpword(const QString &opword)
{
	m_client->sendMessage(net::ServerCommand::make("gain-op", QJsonArray() << opword));
}

/**
 * @brief Generate a reset snapshot and send a reset request
 *
 * The reset image will be sent when the server acknowledges the request.
 * If an empty reset image is given here, one will be generated just in time.
 *
 * If the document is in offline mode, this will immediately reset the current canvas.
 */
void Document::sendResetSession(const drawdance::MessageList &resetImage)
{
	if(!m_client->isConnected()) {
		if(resetImage.isEmpty()) {
			qWarning("Tried to do an offline session reset with a blank reset image");
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
		qInfo("Sending session reset request. (Snapshot size is %lld bytes)", compat::cast<long long>(resetImage.length()));
	}

	m_resetstate = resetImage;
	m_client->sendMessage(net::ServerCommand::make("reset-session"));
}

void Document::sendResizeCanvas(int top, int right, int bottom, int left)
{
	uint8_t contextId = m_client->myId();
	drawdance::Message msgs[] = {
		drawdance::Message::makeUndoPoint(contextId),
		drawdance::Message::makeCanvasResize(contextId, top, right, bottom, left),
	};
	m_client->sendMessages(DP_ARRAY_LENGTH(msgs), msgs);
}

void Document::sendUnban(int entryId)
{
	m_client->sendMessage(net::ServerCommand::makeUnban(entryId));
}

void Document::sendAnnounce(const QString &url, bool privateMode)
{
	m_client->sendMessage(net::ServerCommand::makeAnnounce(url, privateMode));
}

void Document::sendUnannounce(const QString &url)
{
	m_client->sendMessage(net::ServerCommand::makeUnannounce(url));
}

void Document::sendTerminateSession()
{
	m_client->sendMessage(net::ServerCommand::make("kill-session"));
}

void Document::sendCanvasBackground(const QColor &color)
{
	uint8_t contextId = m_client->myId();
	drawdance::Message msgs[] = {
		drawdance::Message::makeUndoPoint(contextId),
		drawdance::Message::makeCanvasBackground(contextId, color),
	};
	m_client->sendMessages(DP_ARRAY_LENGTH(msgs), msgs);
}

void Document::sendAbuseReport(int userId, const QString &message)
{
	QJsonObject kwargs;
	if(userId > 0 && userId < 256)
		kwargs["user"] = userId;
	kwargs["reason"] = message;
	m_client->sendMessage(net::ServerCommand::make("report", QJsonArray(), kwargs));
}

void Document::snapshotNeeded()
{
	// (We) requested a session reset and the server is now ready for it.
	if(m_canvas) {
		if(m_resetstate.isEmpty()) {
			// FunctionRunnable has autoDelete enabled
			auto runnable = new utils::FunctionRunnable([this](){
				generateJustInTimeSnapshot();
			});
			QThreadPool::globalInstance()->start(runnable);
		} else {
			sendResetSnapshot();
		}
	} else {
		qWarning("Server requested snapshot, but canvas is not yet initialized!");
		m_client->sendMessage(net::ServerCommand::make("init-cancel"));
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
	if(m_sessionHistoryMaxSize > 0 && m_resetstate.length() > m_sessionHistoryMaxSize) {
		qWarning("Reset snapshot (%lld) is larger than the size limit (%d)!", compat::cast<long long>(m_resetstate.length()), m_sessionHistoryMaxSize);
		emit autoResetTooLarge(m_sessionHistoryMaxSize);
		m_client->sendMessage(net::ServerCommand::make("init-cancel"));
	} else {
		// Send the reset command+image
		m_client->sendResetMessage(net::ServerCommand::make("init-begin"));
		m_client->sendResetMessages(m_resetstate.count(), m_resetstate.constData());
		m_client->sendResetMessage(net::ServerCommand::make("init-complete"));
	}
	m_resetstate.clear();
}

void Document::undo()
{
	if(!m_canvas)
		return;

	if(!m_toolctrl->undoMultipartDrawing()) {
		m_client->sendMessage(
			drawdance::Message::makeUndo(m_client->myId(), 0, false));
	}
}

void Document::redo()
{
	if(!m_canvas)
		return;

	// Cannot redo while a multipart drawing action is in progress
	if(!m_toolctrl->isMultipartDrawing()) {
		m_client->sendMessage(
			drawdance::Message::makeUndo(m_client->myId(), 0, true));
	}
}

void Document::selectAll()
{
	if(!m_canvas)
		return;

	canvas::Selection *selection = new canvas::Selection;
	selection->setShapeRect(QRect(QPoint(), m_canvas->size()));
	selection->closeShape();
	m_canvas->setSelection(selection);
}

void Document::selectNone()
{
	if(m_canvas && m_canvas->selection() && m_canvas->selection()->pasteOrMoveToCanvas(
			m_messageBuffer, m_client->myId(), m_toolctrl->activeLayer(),
			m_toolctrl->selectInterpolation(), m_client->isCompatibilityMode())) {
		m_client->sendMessages(m_messageBuffer.count(), m_messageBuffer.constData());
		m_messageBuffer.clear();
	}
	cancelSelection();
}

void Document::cancelSelection()
{
	if(m_canvas)
		m_canvas->setSelection(nullptr);
}

void Document::copyFromLayer(int layer)
{
	if(!m_canvas) {
		qWarning("copyFromLayer: no canvas!");
		return;
	}

	QMimeData *data = new QMimeData;
	const auto img = m_canvas->selectionToImage(layer);
	data->setImageData(img);

	// Store also original coordinates
	QPoint srcpos;
	if(m_canvas->selection()) {
		const auto br = m_canvas->selection()->boundingRect();
		srcpos = br.topLeft() + QPoint {
			img.width() / 2,
			img.height() / 2
		};

	} else {
		const QSize s = m_canvas->size();
		srcpos = QPoint(s.width()/2, s.height()/2);
	}

	QByteArray srcbuf =
		QByteArray::number(srcpos.x()) + "," +
		QByteArray::number(srcpos.y()) + "," +
		QByteArray::number(qApp->applicationPid()) + "," +
		QByteArray::number(pasteId());
	data->setData("x-drawpile/pastesrc", srcbuf);

	QGuiApplication::clipboard()->setMimeData(data);
}

bool Document::saveSelection(const QString &path)
{
	QImage img = m_canvas->selectionToImage(0);

	// Fill transparent pixels (i.e. area outside the selection)
	// with the canvas background color.
	// TODO background pattern support
	const auto bg = m_canvas->paintEngine()->backgroundColor();
	if(bg.isValid()) {
		QPainter p(&img);
		p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		p.fillRect(0, 0, img.width(), img.height(), bg);
	}

	return img.save(path);
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
	if(m_canvas) {
		copyFromLayer(m_toolctrl->activeLayer());
		clearArea();
	}
}

void Document::pasteImage(const QImage &image, const QPoint &point, bool forcePoint)
{
	if(m_canvas) {
		m_canvas->pasteFromImage(image, point, forcePoint);
	}
}

void Document::stamp()
{
	canvas::Selection *sel = m_canvas ? m_canvas->selection() : nullptr;
	if(sel && !sel->pasteImage().isNull() && sel->pasteOrMoveToCanvas(
			m_messageBuffer, m_client->myId(), m_toolctrl->activeLayer(),
			m_toolctrl->selectInterpolation(), m_client->isCompatibilityMode())) {
		m_client->sendMessages(m_messageBuffer.count(), m_messageBuffer.constData());
		m_messageBuffer.clear();
		sel->detachMove();
	}
}

void Document::clearArea()
{
	if(m_canvas) {
		canvas::Selection *sel = m_canvas->selection();
		if(sel && (sel->isMovedFromCanvas() || !sel->hasPasteImage())) {
			fillArea(Qt::white, DP_BLEND_MODE_ERASE, sel->isMovedFromCanvas());
		}
		m_canvas->setSelection(nullptr);
	}
}

void Document::fillArea(const QColor &color, DP_BlendMode mode, bool source)
{
	if(!m_canvas) {
		qWarning("fillArea: no canvas!");
		return;
	}
	if(m_canvas->selection() && !m_canvas->aclState()->isLayerLocked(m_toolctrl->activeLayer())
			&& m_canvas->selection()->fillCanvas(
				m_messageBuffer, m_client->myId(), color, mode, m_toolctrl->activeLayer(), source)) {
		m_client->sendMessages(m_messageBuffer.count(), m_messageBuffer.constData());
		m_messageBuffer.clear();
	}
}

void Document::removeEmptyAnnotations()
{
	if(!m_canvas) {
		qWarning("removeEmptyAnnotations(): no canvas");
		return;
	}

	uint8_t contextId = m_canvas->localUserId();
	drawdance::AnnotationList al = m_canvas->paintEngine()->historyCanvasState().annotations();
	int count = al.count();
	for(int i = 0; i < count; ++i) {
		drawdance::Annotation a = al.at(i);
		if(a.textBytes().length() == 0) {
			m_messageBuffer.append(
				drawdance::Message::makeAnnotationDelete(contextId, a.id()));
		}
	}

	m_client->sendMessages(m_messageBuffer.count(), m_messageBuffer.constData());
	m_messageBuffer.clear();
}

void Document::addServerLogEntry(const QString &log)
{
	int i = m_serverLog->rowCount();
	m_serverLog->insertRow(i);
	m_serverLog->setData(m_serverLog->index(i), log);
}

void Document::updateSettings()
{
	if(m_canvas) {
		canvas::PaintEngine *paintEngine = m_canvas->paintEngine();
		paintEngine->setFps(m_settings.engineFrameRate());
		paintEngine->setSnapshotMaxCount(m_settings.engineSnapshotCount());
		paintEngine->setSnapshotMinDelayMs(m_settings.engineSnapshotInterval() * 1000LL);
	}
}
