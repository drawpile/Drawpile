/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2021 Calle Laakkonen

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

#include "document.h"

#include "net/client.h"
#include "net/servercmd.h"
#include "net/banlistmodel.h"
#include "net/announcementlist.h"
#include "canvas/canvasmodel.h"
#include "canvas/paintengine.h"
#include "canvas/selection.h"
#include "canvas/layerlist.h"
#include "canvas/userlist.h"
#include "export/canvassaverrunnable.h"
#include "tools/toolcontroller.h"
#include "utils/images.h"
#include "../libshared/util/functionrunnable.h"

#include <QGuiApplication>
#include <QSettings>
#include <QTimer>
#include <QDir>
#include <QClipboard>
#include <QThreadPool>
#include <QPainter>
#include <QtEndian>
#include <QRunnable>

Document::Document(QObject *parent)
	: QObject(parent),
	  m_resetstate(),
	  m_messageBuffer(),
	  m_canvas(nullptr),
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
	m_banlist = new net::BanlistModel(this);
	m_announcementlist = new net::AnnouncementListModel(this);
	m_serverLog = new QStringListModel(this);

	m_autosaveTimer = new QTimer(this);
	m_autosaveTimer->setSingleShot(true);
	connect(m_autosaveTimer, &QTimer::timeout, this, &Document::autosaveNow);

	// Make connections
	connect(m_client, &net::Client::serverConnected, this, &Document::serverConnected);
	connect(m_client, &net::Client::serverLoggedin, this, &Document::onServerLogin);
	connect(m_client, &net::Client::serverDisconnected, this, &Document::onServerDisconnect);
	connect(m_client, &net::Client::serverDisconnected, this, &Document::serverDisconnected);

	connect(m_client, &net::Client::needSnapshot, this, &Document::snapshotNeeded);
	connect(m_client, &net::Client::sessionConfChange, this, &Document::onSessionConfChanged);
	connect(m_client, &net::Client::autoresetRequested, this, &Document::onAutoresetRequested);
	connect(m_client, &net::Client::serverLog, this, &Document::addServerLogEntry);

	connect(m_client, &net::Client::sessionResetted, this, &Document::onSessionResetted);

	connect(this, &Document::justInTimeSnapshotGenerated, this, &Document::sendResetSnapshot, Qt::QueuedConnection);
}

static void getPaintEngineSettings(
	int &outFps, int &outSnapshotMaxCount, long long &outSnapshotMinDelayMs)
{
	QSettings settings;
	settings.beginGroup("settings/paintengine");
	outFps = settings.value(
		"fps", canvas::PaintEngine::DEFAULT_FPS).toInt();
	outSnapshotMaxCount = settings.value(
		"snapshotcount", canvas::PaintEngine::DEFAULT_SNAPSHOT_MAX_COUNT).toInt();
	outSnapshotMinDelayMs = settings.value(
		"snapshotinterval", canvas::PaintEngine::DEFAULT_SNAPSHOT_MIN_DELAY_MS / 1000).toInt() * 1000LL;
}

void Document::initCanvas()
{
	delete m_canvas;

	int fps, snapshotMaxCount;
	long long snapshotMinDelayMs;
	getPaintEngineSettings(fps, snapshotMaxCount, snapshotMinDelayMs);
	m_canvas = new canvas::CanvasModel{
		m_client->myId(), fps, snapshotMaxCount, snapshotMinDelayMs,
		m_wantCanvasHistoryDump, this};

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

	// Clear out the canvas in preparation for the new data that is about to follow
	m_canvas->resetCanvas();
	m_resetstate.clear();
}

bool Document::loadBlank(const QSize &size, const QColor &background)
{
	setAutosave(false);
	initCanvas();
	unmarkDirty();

	m_canvas->loadBlank(size, background);
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
		m_canvas->loadCanvasState(canvasState);
		setCurrentFilename(path);
		return DP_LOAD_RESULT_SUCCESS;
	}
}

DP_LoadResult Document::loadRecording(const QString &path)
{
	DP_LoadResult result;
	DP_Player *player = DP_load_recording(path.toUtf8().constData(), &result);
	switch (result) {
	case DP_LOAD_RESULT_SUCCESS:
		setAutosave(false);
		initCanvas();
		unmarkDirty();
		m_canvas->loadPlayer(player);
		setCurrentFilename(path);
		return result;
	default:
		Q_ASSERT(!player);
		return result;
	}
}

void Document::onServerLogin(bool join)
{
	if(join)
		initCanvas();

	Q_ASSERT(m_canvas);

	m_canvas->connectedToServer(m_client->myId(), join);

	if(!m_recordOnConnect.isEmpty()) {
		m_originalRecordingFilename = m_recordOnConnect;
		startRecording(m_recordOnConnect);
		m_recordOnConnect = QString();
	}

	m_sessionHistoryMaxSize = 0;
	m_baseResetThreshold = 0;
	emit serverLoggedIn(join);
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

	if(QSettings().value("settings/server/autoreset", true).toBool()) {
		if(query) {
			// This is just a query: send back an affirmative response
			m_client->sendMessage(net::ServerCommand::make("ready-to-autoreset"));

		} else {
			// Autoreset on request
			// TODO: There seems to be a race condition in the server where it
			// will fail to actually send this command out to the clients if the
			// subsequent server command comes in too fast.
			sendLockSession(true);

			QByteArray bytes = QStringLiteral("beginning session autoreset...").toUtf8();
			m_client->sendMessage(drawdance::Message::noinc(DP_msg_chat_new(
				m_client->myId(), DP_MSG_CHAT_TFLAGS_BYPASS, DP_MSG_CHAT_OFLAGS_ACTION,
				bytes.constData(), bytes.length())));

			sendResetSession();
		}
	} else {
		qInfo("Ignoring autoreset request as configured.");
	}
}

void Document::onMoveLayerRequested(int sourceId, int targetId, bool intoGroup, bool below)
{
	uint8_t contextId = m_client->myId();
	drawdance::Message layerOrderMessage = m_canvas->paintEngine()->historyCanvasState()
		.makeLayerOrder(contextId, sourceId, targetId, intoGroup, below);
	if(layerOrderMessage.isNull()) {
		qWarning("Can't move layer: %s", DP_error());
	} else {
		drawdance::Message messages[] = {
			layerOrderMessage,
			drawdance::Message::noinc(DP_msg_undo_point_new(contextId)),
		};
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

void Document::autosave()
{
	if(!m_autosaveTimer->isActive()) {
		const int autosaveInterval = qMax(0, QSettings().value("settings/autosave", 5000).toInt());
		m_autosaveTimer->start(autosaveInterval);
	}
}

void Document::autosaveNow()
{
	if(!isDirty() || !isAutosave() || m_saveInProgress)
		return;

	Q_ASSERT(utils::isWritableFormat(currentFilename()));

	saveCanvas();
}

void Document::saveCanvas(const QString &filename)
{
	setCurrentFilename(filename);
	saveCanvas();
}

void Document::saveCanvas()
{
	Q_ASSERT(!m_saveInProgress);
	m_saveInProgress = true;

	auto *saver = new CanvasSaverRunnable(m_canvas->paintEngine(), m_currentFilename);
	unmarkDirty();
	connect(saver, &CanvasSaverRunnable::saveComplete, this, &Document::onCanvasSaved);
	emit canvasSaveStarted();
	QThreadPool::globalInstance()->start(saver);
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
	m_client->sendMessage(drawdance::Message::noinc(DP_msg_move_pointer_new(
		m_client->myId(), point.x(), point.y())));
}

void Document::sendSessionConf(const QJsonObject &sessionconf)
{
	m_client->sendMessage(net::ServerCommand::make("sessionconf", QJsonArray(), sessionconf));
}

static void setFeatureTiers(int count, uint8_t *out, void *tiers)
{
	memcpy(out, tiers, count * sizeof(*out));
}

void Document::sendFeatureAccessLevelChange(const uint8_t tiers[DP_FEATURE_COUNT])
{
	m_client->sendMessage(drawdance::Message::noinc(DP_msg_feature_access_levels_new(
		m_client->myId(), setFeatureTiers, DP_FEATURE_COUNT, const_cast<uint8_t *>(tiers))));
}

void Document::sendLockSession(bool lock)
{
	m_client->sendMessage(drawdance::Message::noinc(DP_msg_layer_acl_new(
		m_client->myId(), 0, lock ? DP_ACL_ALL_LOCKED_BIT : 0, nullptr, 0, nullptr)));
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
		m_client->sendMessages(resetImage.count(), resetImage.data());
		return;
	}

	if(resetImage.isEmpty()) {
		qInfo("Sending session reset request. (Just in time snapshot)");
	} else {
		qInfo("Sending session reset request. (Snapshot size is %d bytes)", resetImage.length());
	}

	m_resetstate = resetImage;
	m_client->sendMessage(net::ServerCommand::make("reset-session"));
}

void Document::sendResizeCanvas(int top, int right, int bottom, int left)
{
	drawdance::Message msgs[] = {
		drawdance::Message::noinc(DP_msg_undo_point_new(m_client->myId())),
		drawdance::Message::noinc(DP_msg_canvas_resize_new(m_client->myId(), top, right, bottom, left)),
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

static void setBackgroundColor(size_t size, unsigned char *out, void *user)
{
	memcpy(out, user, size);
}

void Document::sendCanvasBackground(const QColor &color)
{
	uint32_t c = qToBigEndian(color.rgba());
	drawdance::Message msgs[] = {
		drawdance::Message::noinc(DP_msg_undo_point_new(m_client->myId())),
		drawdance::Message::noinc(DP_msg_canvas_background_new(
			m_client->myId(), setBackgroundColor, 4, &c)),
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
	m_resetstate = m_canvas->generateSnapshot();
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
		qWarning("Reset snapshot (%d) is larger than the size limit (%d)!", m_resetstate.length(), m_sessionHistoryMaxSize);
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
		m_client->sendMessage(drawdance::Message::noinc(
			DP_msg_undo_new(m_client->myId(), 0, false)));
	}
}

void Document::redo()
{
	if(!m_canvas)
		return;

	// Cannot redo while a multipart drawing action is in progress
	if(!m_toolctrl->isMultipartDrawing()) {
		m_client->sendMessage(drawdance::Message::noinc(
			DP_msg_undo_new(m_client->myId(), 0, true)));
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
			m_messageBuffer, m_client->myId(), m_toolctrl->activeLayer())) {
		m_client->sendMessages(m_messageBuffer.count(), m_messageBuffer.constData());
		m_messageBuffer.clear();
		cancelSelection();
	}
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
		if(m_canvas->selection() && m_canvas->selection()->isMovedFromCanvas())
			m_canvas->selection()->setShape(m_canvas->selection()->moveSourceRegion());

		fillArea(Qt::white, DP_BLEND_MODE_ERASE);
		m_canvas->setSelection(nullptr);
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
			m_messageBuffer, m_client->myId(), m_toolctrl->activeLayer())) {
		m_client->sendMessages(m_messageBuffer.count(), m_messageBuffer.constData());
		m_messageBuffer.clear();
		sel->detachMove();
	}
}

void Document::fillArea(const QColor &color, DP_BlendMode mode)
{
	if(!m_canvas) {
		qWarning("fillArea: no canvas!");
		return;
	}
	if(m_canvas->selection() && !m_canvas->aclState()->isLayerLocked(m_toolctrl->activeLayer())
			&& m_canvas->selection()->fillCanvas(
				m_messageBuffer, m_client->myId(), color, mode, m_toolctrl->activeLayer())) {
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
			m_messageBuffer.append(drawdance::Message::noinc(
				DP_msg_annotation_delete_new(contextId, a.id())));
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
		int fps, snapshotMaxCount;
		long long snapshotMinDelayMs;
		getPaintEngineSettings(fps, snapshotMaxCount, snapshotMinDelayMs);
		canvas::PaintEngine *paintEngine = m_canvas->paintEngine();
		paintEngine->setFps(fps);
		paintEngine->setSnapshotMaxCount(snapshotMaxCount);
		paintEngine->setSnapshotMinDelayMs(snapshotMinDelayMs);
	}
}
