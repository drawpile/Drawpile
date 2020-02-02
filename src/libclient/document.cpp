/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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
#include "net/commands.h"
#include "net/banlistmodel.h"
#include "net/announcementlist.h"
#include "canvas/canvasmodel.h"
#include "canvas/statetracker.h"
#include "canvas/layerlist.h"
#include "canvas/aclfilter.h"
#include "canvas/loader.h"
#include "canvas/userlist.h"
#include "canvas/canvassaverrunnable.h"
#include "canvas/loader.h"
#include "tools/toolcontroller.h"
#include "utils/images.h"

#include "../libshared/net/control.h"
#include "../libshared/net/meta.h"
#include "../libshared/net/meta2.h"
#include "../libshared/net/undo.h"
#include "../libshared/net/layer.h"
#include "../libshared/net/image.h"
#include "../libshared/net/annotation.h"
#include "../libshared/record/writer.h"
#include "../libshared/util/filename.h"

#include <QGuiApplication>
#include <QSettings>
#include <QTimer>
#include <QDir>
#include <QClipboard>
#include <QThreadPool>

Document::Document(QObject *parent)
	: QObject(parent),
	  m_canvas(nullptr),
	  m_recorder(nullptr),
	  m_dirty(false),
	  m_autosave(false),
	  m_canAutosave(false),
	  m_saveInProgress(false),
	  m_sessionPersistent(false),
	  m_sessionClosed(false),
	  m_sessionAuthOnly(false),
	  m_sessionPreserveChat(false),
	  m_sessionPasswordProtected(false),
	  m_sessionOpword(false),
	  m_sessionNsfm(false),
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
}

Document::~Document()
{
	// Cleanly shut down the recording writer if its still active
	if(m_recorder)
		m_recorder->close();
}

void Document::initCanvas()
{
	delete m_canvas;
	m_canvas = new canvas::CanvasModel(m_client->myId(), this);

	m_toolctrl->setModel(m_canvas);

	connect(m_client, &net::Client::messageReceived, m_canvas, &canvas::CanvasModel::handleCommand);
	connect(m_client, &net::Client::drawingCommandLocal, m_canvas, &canvas::CanvasModel::handleLocalCommand);
	connect(m_canvas, &canvas::CanvasModel::canvasModified, this, &Document::markDirty);
	connect(m_canvas->layerlist(), &canvas::LayerListModel::layerCommand, m_client, &net::Client::sendMessage);
	connect(m_canvas, &canvas::CanvasModel::titleChanged, this, &Document::sessionTitleChanged);
	connect(qApp, SIGNAL(settingsChanged()), m_canvas, SLOT(updateLayerViewOptions()));

	connect(m_canvas->stateTracker(), &canvas::StateTracker::catchupProgress, this, &Document::catchupProgress);

	emit canvasChanged(m_canvas);

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

	if(isRecording()) {
		// Resetting establishes a new initial state for the canvas, therefore
		// recording must also be restarted.
		const QString newRecordingFile = utils::makeFilenameUnique(m_originalRecordingFilename, ".dprec");
		m_recorder->close();
		delete m_recorder;
		m_recorder = nullptr;
		emit recorderStateChanged(false);
		startRecording(newRecordingFile, protocol::MessageList(), nullptr);
	}
}

bool Document::loadCanvas(canvas::SessionLoader &loader)
{
	const auto init = loader.loadInitCommands();

	if(init.isEmpty())
		return false;

	setAutosave(false);
	initCanvas();
	m_canvas->layerStack()->setDpi(loader.dotsPerInch());
	m_client->sendResetMessages(init);
	setCurrentFilename(loader.filename());
	unmarkDirty();
	return true;
}

void Document::onServerLogin(bool join)
{
	if(join)
		initCanvas();

	Q_ASSERT(m_canvas);

	m_canvas->connectedToServer(m_client->myId(), join);

	if(!m_recordOnConnect.isEmpty()) {
		m_originalRecordingFilename = m_recordOnConnect;
		startRecording(
			m_recordOnConnect,
			join ? protocol::MessageList() : m_canvas->generateSnapshot(),
			nullptr
		);
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
			m_client->sendMessage(net::command::serverCommand("ready-to-autoreset"));

		} else {
			// Autoreset on request
			sendLockSession(true);
			m_client->sendMessage(protocol::Chat::action(m_client->myId(), "beginning session autoreset...", true));

			sendResetSession(protocol::MessageList());
		}
	} else {
		qInfo("Ignoring autoreset request as configured.");
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
		m_client->setRecordedChatMode(pc);
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

	auto *saver = new canvas::CanvasSaverRunnable(m_canvas, m_currentFilename);
	unmarkDirty();
	connect(saver, &canvas::CanvasSaverRunnable::saveComplete, this, &Document::onCanvasSaved);
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

bool Document::startRecording(const QString &filename, QString *error)
{
	// Set file suffix if missing
	m_originalRecordingFilename = filename;
	const QFileInfo info(filename);
	if(info.suffix().isEmpty())
		m_originalRecordingFilename += ".dprec";

	return startRecording(
		m_originalRecordingFilename,
		m_canvas->generateSnapshot(),
		error
	);
}

bool Document::startRecording(const QString &filename, const protocol::MessageList &initialState, QString *error)
{
	Q_ASSERT(!isRecording());

	m_recorder = new recording::Writer(filename, this);

	if(!m_recorder->open()) {
		qWarning("Couldn't start recording: %s", qPrintable(m_recorder->errorString()));
		if(error)
			*error = m_recorder->errorString();
		delete m_recorder;
		m_recorder = nullptr;
		return false;

	}

	m_recorder->writeHeader();

	for(const protocol::MessagePtr &ptr : initialState) {
		m_recorder->recordMessage(ptr);
	}

	QSettings cfg;
	cfg.beginGroup("settings/recording");
	if(cfg.value("recordpause", true).toBool())
		m_recorder->setMinimumInterval(int(1000 * cfg.value("minimumpause", 0.5).toFloat()));
	if(cfg.value("recordtimestamp", false).toBool())
		m_recorder->setTimestampInterval(1000 * 60 * cfg.value("timestampinterval", 15).toInt());

	m_canvas->setRecorder(m_recorder);

	m_recorder->setAutoflush();
	emit recorderStateChanged(true);
	return true;
}

void Document::stopRecording()
{
	if(!isRecording())
		return;

	m_recorder->close();
	delete m_recorder;
	m_recorder = nullptr;

	emit recorderStateChanged(false);
}

bool Document::saveAsRecording(const QString &filename, QJsonObject header, QString *error) const
{
	recording::Writer writer(filename);

	if(!writer.open()) {
		qWarning("Couldn't open writer: %s", qPrintable(writer.errorString()));
		if(error)
			*error = writer.errorString();
		return false;
	}

	const int initialUserId = 1;

	if(!header.contains("maxUserCount") && sessionMaxUserCount() > 1)
		header["maxUserCount"] = sessionMaxUserCount();

	if(!header.contains("founder"))
		header["founder"] = m_canvas->userlist()->getUsername(m_canvas->localUserId());

	if(!header.contains("title") && !sessionTitle().isEmpty())
		header["title"] = sessionTitle();

	if(!header.contains("nsfm") && isSessionNsfm())
		header["nsfm"] = true;

	if(!header.contains("deputies") && isSessionDeputies())
		header["deputies"] = true;

	if(!header.contains("persistent") && isSessionPersistent())
		header["persistent"] = true;

	if(!header.contains("preserveChat") && isSessionPreserveChat())
		header["preserveChat"] = true;

	writer.writeHeader(header);

	// This recording will probably be used as a session template, so we need to
	// set the session owner as well
	writer.writeMessage(protocol::SessionOwner(0, QList<uint8_t> { initialUserId }));

	auto loader =  canvas::SnapshotLoader(
		initialUserId,
		m_canvas->layerStack(),
		m_canvas->aclFilter());
	loader.setDefaultLayer(m_canvas->layerlist()->defaultLayer());
	loader.setPinnedMessage(m_canvas->pinnedMessage());

	const auto snapshot = loader.loadInitCommands();
	for(const protocol::MessagePtr &ptr : snapshot) {
		writer.writeMessage(*ptr);
	}

	writer.close();
	return true;
}

void Document::sendPointerMove(const QPointF &point)
{
	m_client->sendMessage(protocol::MessagePtr(new protocol::MovePointer(m_client->myId(), int32_t(point.x() * 4), int32_t(point.y() * 4))));
}

void Document::sendSessionConf(const QJsonObject &sessionconf)
{
	m_client->sendMessage(net::command::serverCommand("sessionconf", QJsonArray(), sessionconf));
}

void Document::sendFeatureAccessLevelChange(const uint8_t tiers[canvas::FeatureCount])
{
	static_assert(canvas::FeatureCount == protocol::FeatureAccessLevels::FEATURES, "Feature tier count mismatch");
	m_client->sendMessage(protocol::MessagePtr(new protocol::FeatureAccessLevels(m_client->myId(), tiers)));
}

void Document::sendLockSession(bool lock)
{
	m_client->sendMessage(protocol::MessagePtr(new protocol::LayerACL(m_client->myId(), 0, lock, 0, QList<uint8_t>())));
}

void Document::sendOpword(const QString &opword)
{
	m_client->sendMessage(net::command::serverCommand("gain-op", QJsonArray() << opword));
}

/**
 * @brief Generate a reset snapshot and send a reset request
 *
 * @param resetImage If not empty, this reset image will be used
 * @return true on success
 */
void Document::sendResetSession(const protocol::MessageList &resetImage)
{
	if(resetImage.isEmpty()) {
		qInfo("Sending session reset request. Reset snapshot will be prepared when ready.");
	}

	m_resetstate = resetImage;
	m_client->sendMessage(net::command::serverCommand("reset-session"));
}

void Document::sendResizeCanvas(int top, int right, int bottom, int left)
{
	protocol::MessageList msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(m_client->myId()));
	msgs << protocol::MessagePtr(new protocol::CanvasResize(m_client->myId(), top, right, bottom, left));
	m_client->sendMessages(msgs);
}

void Document::sendUnban(int entryId)
{
	m_client->sendMessage(net::command::unban(entryId));
}

void Document::sendAnnounce(const QString &url, bool privateMode)
{
	m_client->sendMessage(net::command::announce(url, privateMode));
}

void Document::sendUnannounce(const QString &url)
{
	m_client->sendMessage(net::command::unannounce(url));
}

void Document::sendTerminateSession()
{
	m_client->sendMessage(net::command::terminateSession());
}

void Document::sendCanvasBackground(const QColor &color)
{
	m_client->sendMessage(protocol::MessagePtr(new protocol::UndoPoint(m_client->myId())));
	m_client->sendMessage(net::command::setCanvasBackground(m_client->myId(), color));
}

void Document::sendAbuseReport(int userId, const QString &message)
{
	QJsonObject kwargs;
	if(userId > 0 && userId < 256)
		kwargs["user"] = userId;
	kwargs["reason"] = message;
	m_client->sendMessage(net::command::serverCommand("report", QJsonArray(), kwargs));
}

void Document::snapshotNeeded()
{
	// (We) requested a session reset and the server is now ready for it.
	if(m_canvas) {
		if(m_resetstate.isEmpty()) {
			qInfo("Generating snapshot for session reset...");
			if(m_canvas->layerStack()->size().isEmpty()) {
				qWarning("Canvas has no size! Cannot generate reset snapshot!");
				m_client->sendMessage(net::command::serverCommand("init-cancel"));
				return;
			}
			 auto loader = canvas::SnapshotLoader(
				m_client->myId(),
				m_canvas->layerStack(),
				m_canvas->aclFilter());
			 loader.setDefaultLayer(m_canvas->layerlist()->defaultLayer());
			 loader.setPinnedMessage(m_canvas->pinnedMessage());

			 m_resetstate = loader.loadInitCommands();
		}

		// Size limit check. The server will kick us if we send an oversized reset.
		if(m_sessionHistoryMaxSize>0) {
			int resetsize = 0;
			for(protocol::MessagePtr msg : m_resetstate)
				resetsize += msg->length();

			if(resetsize > m_sessionHistoryMaxSize) {
				qWarning("Reset snapshot (%d) is larger than the size limit (%d)!", resetsize, m_sessionHistoryMaxSize);
				emit autoResetTooLarge(m_sessionHistoryMaxSize);
				m_resetstate.clear();
				m_client->sendMessage(net::command::serverCommand("init-cancel"));
				return;
			}
		}

		m_client->sendMessage(net::command::serverCommand("init-begin"));
		m_client->sendResetMessages(m_resetstate);
		m_client->sendMessage(net::command::serverCommand("init-complete"));

		m_resetstate = protocol::MessageList();

	} else {
		qWarning("Server requested snapshot, but canvas is not yet initialized!");
		m_client->sendMessage(net::command::serverCommand("init-cancel"));
	}
}

void Document::undo()
{
	if(!m_canvas)
		return;

	if(!m_toolctrl->undoMultipartDrawing())
		m_client->sendMessage(protocol::MessagePtr(new protocol::Undo(m_client->myId(), 0, false)));
}

void Document::redo()
{
	if(!m_canvas)
		return;

	// Cannot redo while a multipart drawing action is in progress
	if(!m_toolctrl->isMultipartDrawing())
		m_client->sendMessage(protocol::MessagePtr(new protocol::Undo(m_client->myId(), 0, true)));
}

void Document::selectAll()
{
	if(!m_canvas)
		return;

	canvas::Selection *selection = new canvas::Selection;
	selection->setShapeRect(QRect(QPoint(), m_canvas->layerStack()->size()));
	m_canvas->setSelection(selection);
}

void Document::selectNone()
{
	if(m_canvas && m_canvas->selection()) {
		m_client->sendMessages(m_canvas->selection()->pasteOrMoveToCanvas(m_client->myId(), m_toolctrl->activeLayer()));
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
		QSize s = m_canvas->layerStack()->size();
		srcpos = QPoint(s.width()/2, s.height()/2);
	}

	QByteArray srcbuf = QByteArray::number(srcpos.x()) + "," + QByteArray::number(srcpos.y());
	data->setData("x-drawpile/pastesrc", srcbuf);

	QGuiApplication::clipboard()->setMimeData(data);
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

		fillArea(Qt::white, paintcore::BlendMode::MODE_ERASE);
		m_canvas->setSelection(nullptr);
	}
}

void Document::pasteImage(const QImage &image, const QPoint &point, bool forcePoint)
{
	if(!m_canvas) {
		canvas::QImageCanvasLoader loader(image);
		loadCanvas(loader);

	} else {
		m_canvas->pasteFromImage(image, point, forcePoint);
	}
}

void Document::stamp()
{
	canvas::Selection *sel = m_canvas ? m_canvas->selection() : nullptr;
	if(sel && !sel->pasteImage().isNull()) {
		m_client->sendMessages(sel->pasteOrMoveToCanvas(m_client->myId(), m_toolctrl->activeLayer()));
		sel->detachMove();
	}
}

void Document::fillArea(const QColor &color, paintcore::BlendMode::Mode mode)
{
	if(!m_canvas) {
		qWarning("fillArea: no canvas!");
		return;
	}
	if(m_canvas->selection() && !m_canvas->aclFilter()->isLayerLocked(m_toolctrl->activeLayer())) {
		m_client->sendMessages(m_canvas->selection()->fillCanvas(m_client->myId(), color, mode, m_toolctrl->activeLayer()));
	}
}

void Document::removeEmptyAnnotations()
{
	if(!m_canvas) {
		qWarning("removeEmptyAnnotations(): no canvas");
		return;
	}

	QList<uint16_t> ids = m_canvas->layerStack()->annotations()->getEmptyIds();
	if(!ids.isEmpty()) {
		protocol::MessageList msgs;
		msgs << protocol::MessagePtr(new protocol::UndoPoint(m_client->myId()));
		for(auto id : ids)
			msgs << protocol::MessagePtr(new protocol::AnnotationDelete(m_client->myId(), id));
		m_client->sendMessages(msgs);
	}
}

void Document::addServerLogEntry(const QString &log)
{
	int i = m_serverLog->rowCount();
	m_serverLog->insertRow(i);
	m_serverLog->setData(m_serverLog->index(i), log);
}
