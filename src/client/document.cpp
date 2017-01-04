/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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
#include "canvas/canvasmodel.h"
#include "canvas/layerlist.h"
#include "canvas/aclfilter.h"
#include "canvas/loader.h"
#include "canvas/userlist.h"
#include "tools/toolcontroller.h"
#include "utils/settings.h"
#include "utils/images.h"

#include "../shared/net/control.h"
#include "../shared/net/meta2.h"
#include "../shared/net/undo.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/annotation.h"
#include "../shared/record/writer.h"
#include "../shared/util/filename.h"

#include <QApplication>
#include <QSettings>
#include <QTimer>
#include <QDir>
#include <QClipboard>

Document::Document(QObject *parent)
	: QObject(parent),
	  m_canvas(nullptr),
	  m_recorder(nullptr),
	  m_autoRecordOnConnect(false),
	  m_dirty(false),
	  m_autosave(false),
	  m_canAutosave(false),
	  m_sessionPersistent(false),
	  m_sessionClosed(false),
	  m_sessionPreserveChat(false),
	  m_sessionPasswordProtected(false),
	  m_sessionMaxUserCount(0)
{
	// Initialize
	m_client = new net::Client(this);
	m_toolctrl = new tools::ToolController(m_client, this);
	m_banlist = new BanlistModel(this);

	m_autosaveTimer = new QTimer(this);
	m_autosaveTimer->setSingleShot(true);
	connect(m_autosaveTimer, &QTimer::timeout, this, &Document::autosaveNow);

	// Make connections
	connect(m_client, &net::Client::serverConnected, this, &Document::serverConnected);
	connect(m_client, &net::Client::serverLoggedin, this, &Document::onServerLogin);
	connect(m_client, &net::Client::serverLoggedin, this, &Document::serverLoggedin);
	connect(m_client, &net::Client::serverDisconnected, this, &Document::onServerDisconnect);
	connect(m_client, &net::Client::serverDisconnected, this, &Document::serverDisconnected);

	connect(m_client, &net::Client::needSnapshot, this, &Document::snapshotNeeded);
	connect(m_client, &net::Client::sessionConfChange, this, &Document::onSessionConfChanged);
	connect(m_client, &net::Client::serverHistoryLimitReceived, this, &Document::onServerHistoryLimitReceived);
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
	connect(m_client, &net::Client::sessionResetted, [this]() {
		m_canvas->resetCanvas();
		if(m_serverSpaceLow) {
			// Session reset is the only thing that can free up history space
			m_serverSpaceLow = false;
			emit serverSpaceLowChanged(false);
		}
	});
	connect(m_canvas, &canvas::CanvasModel::canvasModified, this, &Document::markDirty);
	connect(m_canvas->layerlist(), &canvas::LayerListModel::layerCommand, m_client, &net::Client::sendMessage);
	connect(m_canvas, &canvas::CanvasModel::titleChanged, this, &Document::sessionTitleChanged);
	connect(qApp, SIGNAL(settingsChanged()), m_canvas, SLOT(updateLayerViewOptions()));

	emit canvasChanged(m_canvas);

	setCurrentFilename(QString());
}

bool Document::loadCanvas(canvas::SessionLoader &loader)
{
	QList<protocol::MessagePtr> init = loader.loadInitCommands();

	if(init.isEmpty())
		return false;

	setAutosave(false);

	initCanvas();

	// Set local history size limit. This must be at least as big as the initializer,
	// otherwise a new snapshot will always have to be generated when hosting a session.
	uint minsizelimit = 0;
	for(protocol::MessagePtr msg : init)
		minsizelimit += msg->length();
	minsizelimit *= 2;

	m_client->sendMessages(init);

	setCurrentFilename(loader.filename());
	unmarkDirty();
	return true;
}

void Document::onServerLogin(bool join)
{
	if(join)
		initCanvas();

	Q_ASSERT(m_canvas);

	m_canvas->connectedToServer(m_client->myId());

	if(m_autoRecordOnConnect) {
		startRecording(utils::uniqueFilename(utils::settings::recordingFolder(), "session-" + m_client->sessionId(), "dprec"));
	}

	m_serverSpaceLow = false;
	emit serverSpaceLowChanged(false);
}

void Document::onServerDisconnect()
{
	if(m_canvas) {
		m_canvas->disconnectedFromServer();
		m_canvas->setTitle(QString());
	}
	m_banlist->clear();
}

void Document::onSessionConfChanged(const QJsonObject &config)
{
	if(config.contains("persistent"))
		setSessionPersistent(config["persistent"].toBool());

	if(config.contains("closed"))
		setSessionClosed(config["closed"].toBool());

	if(config.contains("title"))
		m_canvas->setTitle(config["title"].toString());

	if(config.contains("preserveChat"))
		setSessionPreserveChat(config["preserveChat"].toBool());

	if(config.contains("hasPassword"))
		setSessionPasswordProtected(config["hasPassword"].toBool());

	if(config.contains("maxUserCount"))
		setSessionMaxUserCount(config["maxUserCount"].toInt());

	if(config.contains("banlist"))
		m_banlist->updateBans(config["banlist"].toArray());
}

void Document::onServerHistoryLimitReceived(int maxSpace)
{
	if(!m_serverSpaceLow) {
		m_serverSpaceLow = true;
		emit serverSpaceLowChanged(true);
	}

	if(m_client->myId() == m_canvas->userlist()->getPrimeOp() &&
		QSettings().value("settings/server/autoreset", true).toBool())
	{
		// We're the "prime operator", meaning it's our responsibility
		// to handle the autoreset
		if(!sendResetSession(canvas::StateSavepoint(), maxSpace)) {
			emit autoResetTooLarge(maxSpace);
		}
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

void Document::setSessionMaxUserCount(int count)
{
	if(m_sessionMaxUserCount != count) {
		m_sessionMaxUserCount = count;
		emit sessionMaxUserCountChanged(count);
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

void Document::setAutosave(bool autosave)
{
	if(autosave && !canAutosave()) {
		qWarning("Can't' to autosave");
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

		bool couldAutosave = m_canAutosave;
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
		int autosaveInterval = qMax(0, QSettings().value("settings/autosave", 5000).toInt());
		m_autosaveTimer->start(autosaveInterval);
	}
}

void Document::autosaveNow()
{
	if(!isDirty() || !isAutosave())
		return;

	Q_ASSERT(utils::isWritableFormat(currentFilename()));

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	bool saved = m_canvas->save(currentFilename());
	QApplication::restoreOverrideCursor();

	if(saved)
		unmarkDirty();
	else
		qWarning("Error autosaving");
}

bool Document::saveCanvas(const QString &filename)
{
	if(m_canvas->save(filename)) {
		setCurrentFilename(filename);
		unmarkDirty();
		return true;
	}

	return false;
}

bool Document::startRecording(const QString &filename, QString *error)
{
	Q_ASSERT(!isRecording());

	// Set file suffix if missing
	QString file = filename;
	const QFileInfo info(file);
	if(info.suffix().isEmpty())
		file += ".dprec";

	// Start the recorder
	m_recorder = new recording::Writer(file, this);

	if(!m_recorder->open()) {
		if(error)
			*error = m_recorder->errorString();
		delete m_recorder;
		m_recorder = nullptr;
		return false;

	}

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	m_recorder->writeHeader();

	QList<protocol::MessagePtr> snapshot = m_canvas->generateSnapshot(false);
	for(const protocol::MessagePtr ptr : snapshot) {
		m_recorder->recordMessage(ptr);
	}

	QSettings cfg;
	cfg.beginGroup("settings/recording");
	if(cfg.value("recordpause", true).toBool())
		m_recorder->setMinimumInterval(1000 * cfg.value("minimumpause", 0.5).toFloat());

	connect(m_client, &net::Client::messageReceived, m_recorder, &recording::Writer::recordMessage);

	QApplication::restoreOverrideCursor();

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

void Document::sendPointerMove(const QPointF &point)
{
	m_client->sendMessage(protocol::MessagePtr(new protocol::MovePointer(m_client->myId(), point.x() * 4, point.y() * 4)));
}

// Convenience function
void Document::sendSessionConf(const QString &key, const QJsonValue &value)
{
	QJsonObject kwargs;
	kwargs[key] = value;
	m_client->sendMessage(net::command::serverCommand("sessionconf", QJsonArray(), kwargs));
}

void Document::sendPersistentSession(bool p)
{
	sendSessionConf("persistent", p);
}

void Document::sendCloseSession(bool close)
{
	sendSessionConf("closed", close);
}

void Document::sendPasswordChange(const QString &password)
{
	sendSessionConf("password", password);
}

void Document::sendUserLimitChange(int newLimit)
{
	sendSessionConf("maxUserCount", newLimit);
}

void Document::sendPreserveChatChange(bool keepChat)
{
	sendSessionConf("preserveChat", keepChat);
}

/**
 * @brief Generate a reset snapshot and send a reset request
 *
 * @param savepoint the savepoint from which to generate. Use a null savepoint to generate from the current state
 * @param sizelimit if larger than zero, the snapshot must not exceed this size
 * @return true on success
 */
bool Document::sendResetSession(const canvas::StateSavepoint &savepoint, int sizelimit)
{
	if(!savepoint) {
		qInfo("Preparing session reset from current canvas content");
		m_resetstate = canvas::SnapshotLoader(m_client->myId(), m_canvas->layerStack()).loadInitCommands();
	} else {
		qInfo("Preparing session reset from a savepoint");
		m_resetstate = savepoint.initCommands(m_client->myId());
	}

	if(sizelimit>0) {
		int resetsize = 0;
		for(protocol::MessagePtr msg : m_resetstate)
			resetsize += msg->length();

		if(resetsize > sizelimit) {
			qWarning("Reset snapshot (%d) is larger than the size limit (%d)!", resetsize, sizelimit);
			m_resetstate.clear();
			return false;
		}
	}

	m_client->sendMessage(net::command::serverCommand("reset-session"));
	return true;
}

void Document::sendLockSession(bool lock)
{
	Q_ASSERT(m_canvas);

	uint16_t flags = m_canvas->aclFilter()->sessionAclFlags();
	if(lock)
		flags |= protocol::SessionACL::LOCK_SESSION;
	else
		flags &= ~protocol::SessionACL::LOCK_SESSION;

	m_client->sendMessage(protocol::MessagePtr(new protocol::SessionACL(m_client->myId(), flags)));
}

void Document::sendLockImageCommands(bool lock)
{
	Q_ASSERT(m_canvas);

	uint16_t flags = m_canvas->aclFilter()->sessionAclFlags();
	if(lock)
		flags |= protocol::SessionACL::LOCK_IMAGES;
	else
		flags &= ~protocol::SessionACL::LOCK_IMAGES;

	m_client->sendMessage(protocol::MessagePtr(new protocol::SessionACL(m_client->myId(), flags)));
}

void Document::sendLayerCtrlMode(bool lockCtrl, bool ownLayers)
{
	Q_ASSERT(m_canvas);

	uint16_t flags = m_canvas->aclFilter()->sessionAclFlags();
	flags &= ~(protocol::SessionACL::LOCK_LAYERCTRL|protocol::SessionACL::LOCK_OWNLAYERS);

	if(lockCtrl)
		flags |= protocol::SessionACL::LOCK_LAYERCTRL;
	if(ownLayers)
		flags |= protocol::SessionACL::LOCK_OWNLAYERS;

	m_client->sendMessage(protocol::MessagePtr(new protocol::SessionACL(m_client->myId(), flags)));
}

void Document::sendResizeCanvas(int top, int right, int bottom, int left)
{
	QList<protocol::MessagePtr> msgs;
	msgs << protocol::MessagePtr(new protocol::UndoPoint(m_client->myId()));
	msgs << protocol::MessagePtr(new protocol::CanvasResize(m_client->myId(), top, right, bottom, left));
	m_client->sendMessages(msgs);
}

void Document::sendUnban(int entryId)
{
	m_client->sendMessage(net::command::unban(entryId));
}


void Document::snapshotNeeded()
{
	// (We) requested a session reset and the server is now ready for it.
	if(m_canvas) {
		if(m_resetstate.isEmpty()) {
			qWarning("Session reset snapshot requested, but we have not prepared it! Generating one now...");
			m_resetstate = canvas::SnapshotLoader(m_client->myId(), m_canvas->layerStack()).loadInitCommands();
		}

		m_client->sendMessages(m_resetstate);

		if(!m_client->isLocalServer())
			m_client->sendMessage(net::command::serverCommand("init-complete"));

		m_resetstate = QList<protocol::MessagePtr>();

	} else {
		qWarning("Server requested snapshot, but canvas is not yet initialized!");
	}
}

void Document::undo()
{
	if(!m_canvas)
		return;

	if(m_canvas->selection()) {
		cancelSelection();
	} else {
		m_client->sendMessage(protocol::MessagePtr(new protocol::Undo(m_client->myId(), 0, false)));
	}
}

void Document::redo()
{
	if(!m_canvas)
		return;

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
		m_client->sendMessages(m_canvas->selection()->pasteToCanvas(m_client->myId(), m_toolctrl->activeLayer()));
		cancelSelection();
	}
}

void Document::cancelSelection()
{
	if(m_canvas && m_canvas->selection()) {
		if(!m_canvas->selection()->pasteImage().isNull() && m_canvas->selection()->isMovedFromCanvas())
			m_client->sendMessage(protocol::MessagePtr(new protocol::Undo(m_client->myId(), 0, false)));
		m_canvas->setSelection(nullptr);
	}
}

void Document::copyFromLayer(int layer)
{
	if(!m_canvas) {
		qWarning("copyFromLayer: no canvas!");
		return;
	}

	QMimeData *data = new QMimeData;
	data->setImageData(m_canvas->selectionToImage(layer));

	// Store also original coordinates
	QPoint srcpos;
	if(m_canvas->selection()) {
		srcpos = m_canvas->selection()->boundingRect().center();

	} else {
		QSize s = m_canvas->layerStack()->size();
		srcpos = QPoint(s.width()/2, s.height()/2);
	}

	QByteArray srcbuf = QByteArray::number(srcpos.x()) + "," + QByteArray::number(srcpos.y());
	data->setData("x-drawpile/pastesrc", srcbuf);

	QApplication::clipboard()->setMimeData(data);
}

void Document::copyVisible()
{
	copyFromLayer(0);
}

void Document::copyLayer()
{
	copyFromLayer(m_toolctrl->activeLayer());
}

void Document::cutLayer()
{
	copyFromLayer(m_toolctrl->activeLayer());
	fillArea(Qt::white, paintcore::BlendMode::MODE_ERASE);
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
		m_client->sendMessages(sel->pasteToCanvas(m_client->myId(), m_toolctrl->activeLayer()));
		sel->setMovedFromCanvas(false);
	}
}

void Document::fillArea(const QColor &color, paintcore::BlendMode::Mode mode)
{
	if(!m_canvas) {
		qWarning("fillArea: no canvas!");
		return;
	}
	if(m_canvas->selection()) {
		m_client->sendMessages(m_canvas->selection()->fillCanvas(m_client->myId(), color, mode, m_toolctrl->activeLayer()));
	}
}

void Document::removeEmptyAnnotations()
{
	QList<int> ids = m_canvas->layerStack()->annotations()->getEmptyIds();
	if(!ids.isEmpty()) {
		QList<protocol::MessagePtr> msgs;
		msgs << protocol::MessagePtr(new protocol::UndoPoint(m_client->myId()));
		for(int id : ids)
			msgs << protocol::MessagePtr(new protocol::AnnotationDelete(m_client->myId(), id));
		m_client->sendMessages(msgs);
	}
}
