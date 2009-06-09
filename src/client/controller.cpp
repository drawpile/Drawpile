/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QDebug>
#include <QBuffer>
#include <QUrl>

#include "controller.h"
#include "board.h"
#include "core/brush.h"
#include "tools.h"
#include "boardeditor.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "localserver.h"
#include "toolsettings.h"
#include "user.h"

#include "../shared/net/constants.h" // DEFAULT_PORT

Controller::Controller(tools::AnnotationSettings *as, QObject *parent)
	: QObject(parent), board_(0), session_(0), maxusers_(16), pendown_(false), sync_(false), syncwait_(false), lock_(false)
{
	toolbox_.setAnnotationSettings(as);

	host_ = new network::HostState(this);
	connect(host_, SIGNAL(loggedin()), this, SIGNAL(loggedin()));
	connect(host_, SIGNAL(joined()), this, SLOT(sessionJoined()));

	connect(host_, SIGNAL(needPassword()), this, SIGNAL(needPassword()));

	connect(host_, SIGNAL(error(QString)), this, SIGNAL(netError(QString)));
	
	connect(host_, SIGNAL(disconnected()), this, SLOT(netDisconnected()));
	connect(host_, SIGNAL(connected()), this, SLOT(netConnected()));
}

Controller::~Controller()
{
}

bool Controller::isConnected() const
{
	return host_->isConnected();
}

void Controller::setModel(drawingboard::Board *board)
{
	board_ = board;
	board_->addUser(0);
	board_->setLocalUser(0);
	toolbox_.setEditor( board->getEditor() );
	toolbox_.aeditor()->setBoardEditor( toolbox_.editor() );
	connect(board, SIGNAL(newLocalAnnotation(drawingboard::AnnotationItem*)),
			toolbox_.aeditor(), SLOT(setSelection(drawingboard::AnnotationItem*)));
	connect(board, SIGNAL(annotationDeleted(drawingboard::AnnotationItem*)),
			toolbox_.aeditor(), SLOT(unselect(drawingboard::AnnotationItem*)));
	// TODO testing...
	//toolbox_.editor()->setLayer(1);
}

/**
 * Establish a connection with a server.
 * The connected signal will be emitted when connection is established
 * and loggedin when login is succesful.
 * If a path is specified, the session with a matching name is automatically
 * joined.
 *
 * If hosting on a remote server, administrator password is usually required
 * to create sessions.
 *
 * @param url host url. Should contain an username. May contain a path
 * @param adminpasswd administrator password (needed only in some cases)
 */
void Controller::connectHost(const QUrl& url)
{
	Q_ASSERT(host_ != 0);
	Q_ASSERT(url.userName().isEmpty()==false);

	username_ = url.userName();

	// This is purely cosmetic. This address is displayed to the user when
	// connection is established. Show only the host and port if specified.
	// When connecting to localhost, try to guess the most likely address
	// visible to the outside net.
	if(url.host()=="127.0.0.1")
		address_ = LocalServer::address();
	else
		address_ = url.host();
	if(url.port()>0) // TODO wrap IPV6 address in []
		address_ += ":" + QString::number(url.port());

	// Connect to host
	host_->connectToHost(url.host(), url.port(protocol::DEFAULT_PORT));

	sync_ = false;
	syncwait_ = false;
}

bool Controller::isUploading() const
{
	if(session_)
		return session_->isUploading();
	return false;
}

/**
 * Simply join an existing session upon login. If server has no
 * session, an error message is displayed.
 */
void Controller::joinSession(const QUrl& url) {
	host_->setNoHost();
	connectHost(url);
}

/**
 * A new session is created and joined.
 * @param title session title
 * @param image initial board image
 */
void Controller::hostSession(const QUrl& url, const QString& password,
		const QString& title, const QImage& image, int maxusers, bool allowDraw)
{
	maxusers_ = maxusers; // remember this for deny joins
	host_->setHost(board_->getAnnotations(true),
			password, title, image.width(),
			image.height(), maxusers, allowDraw);
	connectHost(url);
}

/**
 * Send a password to log in or join.
 * @param password password to send
 */
void Controller::sendPassword(const QString& password)
{
	host_->sendPassword(password);
}

void Controller::disconnectHost()
{
	Q_ASSERT(host_);
	host_->disconnectFromHost();
}

void Controller::lockBoard(bool lock)
{
	Q_ASSERT(session_);
	session_->lock(lock);
}

void Controller::disallowJoins(bool disallow)
{
	Q_ASSERT(session_);
	session_->setUserLimit(disallow?1:maxusers_);
}

void Controller::sendChat(const QString& message)
{
	Q_ASSERT(session_);
	session_->sendChat(message);
}

/**
 * Prepare the system for a networked session.
 * @param id session id
 */
void Controller::sessionJoined()
{
	qDebug() << "Joined session";
	const int userid = host_->localUser();
	session_ = host_->session();

	// Delete old annotations. If we are hosting this session, the server
	// should send them right back at us.
	board_->clearAnnotations();

	// Update user list
	board_->clearUsers();
	const QList<int> users = session_->users();
	foreach(int id, users)
		board_->addUser(id);
	board_->setLocalUser(userid);

	// Make session -> controller connections
	connect(session_, SIGNAL(rasterReceived(int)),
			this, SLOT(rasterDownload(int)));
	connect(session_, SIGNAL(rasterSent(int)),
			this, SIGNAL(rasterUploadProgress(int)));
	connect(session_, SIGNAL(syncRequest()),
			this, SLOT(rasterUpload()));
	connect(session_, SIGNAL(syncWait()),
			this, SLOT(syncWait()));
	connect(session_, SIGNAL(syncDone()),
			this, SLOT(syncDone()));
	connect(session_, SIGNAL(userJoined(int)),
			this, SLOT(addUser(int)));
	connect(session_, SIGNAL(userLeft(int)),
			this, SLOT(removeUser(int)));
	connect(session_, SIGNAL(sessionLocked(bool)),
			this, SLOT(sessionLocked(bool)));
	connect(session_, SIGNAL(userLocked(int, bool)),
			this, SLOT(userLocked(int,bool)));
	connect(session_, SIGNAL(userKicked(int)),
			this, SLOT(sessionKicked(int)));
	connect(session_, SIGNAL(chatMessage(QString, QString)),
			this, SIGNAL(chat(QString,QString)));
	connect(session_, SIGNAL(boardChanged()),
			this, SIGNAL(boardChanged()));
			
	// Make session -> board connections
	connect(session_, SIGNAL(toolReceived(int,dpcore::Brush)),
			board_, SLOT(userSetTool(int,dpcore::Brush)));
	connect(session_, SIGNAL(strokeReceived(int,dpcore::Point)),
			board_, SLOT(userStroke(int,dpcore::Point)));
	connect(session_, SIGNAL(strokeReceived(int,dpcore::Point)),
			this, SIGNAL(changed()));
	connect(session_, SIGNAL(strokeEndReceived(int)), board_,
			SLOT(userEndStroke(int)));
	connect(session_, SIGNAL(annotation(const protocol::Annotation&)),
			board_, SLOT(annotate(const protocol::Annotation&)));
	connect(session_, SIGNAL(rmAnnotation(int)),
			board_, SLOT(unannotate(int)));

	// Get a remote board editor
	delete toolbox_.editor();
	toolbox_.setEditor( board_->getEditor(session_) );
	toolbox_.aeditor()->setBoardEditor( toolbox_.editor() );

	emit joined();

	// Set lock
	if(session_->user(userid).locked())
		userLocked(userid, true);
}

/**
 * Restore to local drawing mode.
 */
void Controller::netDisconnected()
{
	// Remove remote users
	board_->clearUsers();
	board_->addUser(0);
	board_->setLocalUser(0);

	// Get a local board editor
	delete toolbox_.editor();
	toolbox_.setEditor( board_->getEditor() );
	toolbox_.aeditor()->setBoardEditor( toolbox_.editor() );

	session_ = 0;
	emit parted();
	if(lock_) {
		lock_ = false;
		emit unlockboard();
	}
	sync_ = false;
	syncwait_ = false;

	emit disconnected(tr("Disconnected"));
}

/**
 * @param id user id
 */
void Controller::addUser(int id)
{
	board_->addUser(id);
	// Ghosted users don't go anywhere but the board.
	network::User user = session_->user(id);
	if(user.name().at(0)!='!')
		emit userJoined(user);
}

/**
 * @param id user id
 */
void Controller::removeUser(int id)
{
	// Users are never removed from the board.
	// This is to make syncing simpler.
	emit userParted(session_->user(id));
}

/**
 * A piece of the board image was received. A signal is emitted with
 * the percentage of total received data. Download is fully completed
 * when rasterDownloadProgress(100) is emitted.
 * @param p download progress [0..100]
 */
void Controller::rasterDownload(int p)
{
	if(p>=100) {
		QImage img;
		if(session_->sessionImage(img)) {
			if(img.isNull()==false) {
				board_->initBoard(img);
				emit changed();
				session_->releaseRaster();
			}
		} else {
			/** @todo downloaded invalid image */
			Q_ASSERT(false);
		}
	}
	emit rasterDownloadProgress(p);
}

/**
 * If the user is currently drawing something, politely wait until
 * the pen is lifted. Otherwise start sending the board contents immediately.
 */
void Controller::rasterUpload()
{
	if(pendown_)
		sync_ = true;
	else
		sendRaster();
}

/**
 * If user is currently drawing something, wait until the pen is lifted.
 * Otherwise lock the board immediately.
 */
void Controller::syncWait()
{
	if(pendown_)
		syncwait_ = true;
	else
		lockForSync();
}

/**
 * Unlock the board after sync, but only if there is no user or session lock.
 */
void Controller::syncDone()
{
	if(session_->user(host_->localUser()).locked()==false &&
			session_->info().lock()==false) {
		emit unlockboard();
		lock_ = false;
	}
}

/**
 * The session is (un)locked, even if the user is in the middle of drawing.
 * The session lock is not lifted however, if a user lock or the general
 * session lock is still in place.
 * @param lock lock or unlock
 */
void Controller::sessionLocked(bool lock)
{
	if(lock) {
		emit lockboard(tr("Locked by session owner"));
		if(pendown_ && tool_->readonly()==false)
			penUp();
		lock_ = true;
	} else {
		const network::User &localuser = session_->user(host_->localUser());
		// Unlock if local user is not locked and general session lock was lifted
		if(localuser.locked()==false && session_->info().lock()==false) {
			emit unlockboard();
			lock_ = false;
		}
	}
}

/**
 * Usually just emits a userChanged signal. If the lock applies to the local
 * user, the session might be locked or unlocked, depending on the status
 * of the general session lock.
 * @param id user id
 * @param lock lock status
 */
void Controller::userLocked(int id, bool lock)
{
	const network::User &user = session_->user(id);

	qDebug() << "user locked" << id << lock;
	emit userChanged(user);

	if(lock) {
		// Lock UI if local user was locked
		if(user.isLocal() && lock_==false)
			sessionLocked(true);
	} else {
		// Unlock UI if local user was unlocked and general session lock
		// is not in place.
		if(user.isLocal() && session_->info().lock()==false)
			sessionLocked(false);
	}
}

/**
 * @param id id of the kicked user
 */
void Controller::sessionKicked(int id)
{
	emit userKicked(session_->user(id));
}

/**
 * Take a copy of the board contents (in PNG format) and give it to
 * the session object for uploading.
 */
void Controller::sendRaster()
{
	Q_ASSERT(session_);
	QByteArray raster;
	QBuffer buffer(&raster);
	board_->image().save(&buffer, "PNG");
	session_->sendRaster(raster);
}

/**
 * The board is locked and the server is informed of the lock.
 * The synchronization lock is usually quite short, it will be released
 * as soon as one user has called sendRaster()
 */
void Controller::lockForSync()
{
	if(lock_==false) {
		emit lockboard(tr("Synchronizing new user"));
		lock_ = true;
	}
	session_->sendAckSync();
}

void Controller::setTool(tools::Type tool)
{
	tool_ = toolbox_.get(tool);
}

void Controller::selectLayer(int id)
{
	toolbox_.editor()->setLayer(id);
}

void Controller::newLayer(const QString& name)
{
	toolbox_.editor()->createLayer(name);
}

void Controller::deleteLayer(int id, bool mergedown)
{
	toolbox_.editor()->deleteLayer(id);
}

void Controller::setLayerOpacity(int id, int opacity)
{
	toolbox_.editor()->changeLayerOpacity(id, opacity);
}

void Controller::penDown(const dpcore::Point& point)
{
	if(lock_ == false || (lock_ && tool_->readonly())) {
		tool_->begin(point);
		if(tool_->readonly()==false) {
			if(board_->hasImage())
				emit changed();
			pendown_ = true;
		}
	}
}

void Controller::penMove(const dpcore::Point& point)
{
	if(lock_ == false || tool_->readonly()) {
		tool_->motion(point);
	}
}

void Controller::penUp()
{
	if(lock_ == false || tool_->readonly()) {
		tool_->end();
		if(sync_) {
			sync_ = false;
			sendRaster();
		}
		if(syncwait_) {
			syncwait_ = false;
			lockForSync();
		}
		pendown_ = false;
	}
}

/**
 * Initiate login procedure and emit a signal informing that the connection
 * was established.
 */
void Controller::netConnected()
{
	emit connected(address_);
	host_->login(username_);
}

