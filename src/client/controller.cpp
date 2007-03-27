/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#include "controller.h"
#include "board.h"
#include "brush.h"
#include "tools.h"
#include "boardeditor.h"
#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "localserver.h"

#include "../shared/protocol.defaults.h"

Controller::Controller(QObject *parent)
	: QObject(parent), board_(0), net_(0), session_(0), pendown_(false), sync_(false), syncwait_(false), lock_(false)
{
	host_ = new network::HostState(this);
	connect(host_, SIGNAL(loggedin()), this, SLOT(serverLoggedin()));
	connect(host_, SIGNAL(becameAdmin()), this, SLOT(finishLogin()));
	connect(host_, SIGNAL(joined(int)), this, SLOT(sessionJoined(int)));
	connect(host_, SIGNAL(parted(int)), this, SLOT(sessionParted()));

	connect(host_, SIGNAL(noSessions()), this, SLOT(disconnectHost()));
	connect(host_, SIGNAL(noSessions()), this, SIGNAL(noSessions()));
	connect(host_, SIGNAL(sessionNotFound()), this, SLOT(disconnectHost()));
	connect(host_, SIGNAL(sessionNotFound()), this, SIGNAL(sessionNotFound()));

	connect(host_, SIGNAL(selectSession(network::SessionList)), this, SIGNAL(selectSession(network::SessionList)));
	connect(host_, SIGNAL(needPassword()), this, SIGNAL(needPassword()));

	connect(host_, SIGNAL(error(QString)), this, SIGNAL(netError(QString)));
	// Disconnect on error
	connect(host_, SIGNAL(error(QString)), this, SLOT(disconnectHost()));
}

Controller::~Controller()
{
}

void Controller::setModel(drawingboard::Board *board)
{
	board_ = board;
	board_->addUser(0);
	board_->setLocalUser(0);
	toolbox_.setEditor( board->getEditor() );
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
void Controller::connectHost(const QUrl& url,const QString& adminpasswd)
{
	Q_ASSERT(net_ == 0);
	Q_ASSERT(url.userName().isEmpty()==false);

	username_ = url.userName();

	// This is purely cosmetic. This address is displayed to the user when
	// connection is established. Show only the host and port if specified.
	address_ = url.toString(QUrl::RemoveScheme|QUrl::RemovePassword|
			QUrl::RemoveUserInfo|QUrl::RemovePath|QUrl::RemoveQuery|
			QUrl::RemoveFragment|QUrl::StripTrailingSlash
			).mid(2);

	// Create network thread object
	net_ = new network::Connection(this);
	connect(net_,SIGNAL(connected()), this, SLOT(netConnected()));
	connect(net_,SIGNAL(disconnected(QString)), this, SLOT(netDisconnected(QString)));
	connect(net_,SIGNAL(error(QString)), this, SIGNAL(netError(QString)));
	connect(net_,SIGNAL(received()), host_, SLOT(receiveMessage()));

	// Autojoin if path is present
	autojoinpath_ = url.path();

	// Become admin if password is set
	adminpasswd_ = adminpasswd;

	// Connect to host
	host_->setConnection(net_);
	QString host = url.host();
	if(host.compare(LocalServer::address())==0) {
		qDebug() << "actually connecting to localhost instead of" << host;
		host = "127.0.0.1"; // server only allows admin users from localhost
	}
	net_->connectHost(host, url.port(protocol::default_port));

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
 * A new session is created and joined.
 * @param title session title
 * @param password session password. If empty, no password is set
 * @param image initial board image
 * @param userlimit max. number of users
 * @param allowdraw allow drawing by default
 * @param allowchat allow chatting by default
 * @pre host connection must be established and user logged in.
 */
void Controller::hostSession(const QString& title, const QString& password,
		const QImage& image, int userlimit, bool allowdraw, bool allowchat)
{
	Q_ASSERT(host_);
	host_->host(title, password, image.width(), image.height(),
			userlimit, allowdraw, allowchat);
}

/**
 * If there is only one session, it is joined automatically. Otherwise a
 * list of sessions is presented to the user to choose from.
 */
void Controller::joinSession()
{
	Q_ASSERT(host_);
	host_->join();
}

/**
 * Send a password to log in or join.
 * @param password password to send
 */
void Controller::sendPassword(const QString& password)
{
	host_->sendPassword(password);
}

void Controller::joinSession(int id)
{
	host_->join(id);
}

void Controller::disconnectHost()
{
	Q_ASSERT(net_);
	net_->disconnectHost();
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

void Controller::serverLoggedin()
{
	if(adminpasswd_.isEmpty())
		finishLogin();
	else
		host_->becomeAdmin(adminpasswd_);
}

void Controller::finishLogin()
{
	emit loggedin();
	if(autojoinpath_.length()>1)
		host_->join(autojoinpath_.mid(1));
}

/**
 * A session was joined
 */
void Controller::sessionJoined(int id)
{
	const int userid = host_->localUser().id();
	session_ = host_->session(id);

	// Remember maximum user count
	maxusers_ = session_->info().maxusers;

	// Update user list
	board_->clearUsers();
	QList<int> users = session_->users();
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
	connect(session_, SIGNAL(ownerChanged()),
			this, SLOT(sessionOwnerChanged()));
	connect(session_, SIGNAL(userKicked(int)),
			this, SLOT(sessionKicked(int)));
	connect(session_, SIGNAL(chatMessage(QString, QString)),
			this, SIGNAL(chat(QString,QString)));
	connect(session_, SIGNAL(userLimitChanged(int)),
			this, SLOT(sessionUserLimitChanged(int)));

	// Make session -> board connections
	connect(session_, SIGNAL(toolReceived(int,drawingboard::Brush)),
			board_, SLOT(userSetTool(int,drawingboard::Brush)));
	connect(session_, SIGNAL(strokeReceived(int,drawingboard::Point)),
			board_, SLOT(userStroke(int,drawingboard::Point)));
	connect(session_, SIGNAL(strokeReceived(int,drawingboard::Point)),
			this, SIGNAL(changed()));
	connect(session_, SIGNAL(strokeEndReceived(int)), board_,
			SLOT(userEndStroke(int)));
	connect(session_, SIGNAL(userJoined(int)), board_,
			SLOT(addUser(int)));
	connect(session_, SIGNAL(userLeft(int)), board_,
			SLOT(removeUser(int)));

	// Get a remote board editor
	delete toolbox_.editor();
	toolbox_.setEditor( board_->getEditor(session_) );

	emit joined(session_);

	// Set lock
	if(session_->user(userid).locked())
		userLocked(userid, true);
}

/**
 * A session was left
 */
void Controller::sessionParted()
{
	// Remove remote users
	board_->clearUsers();
	board_->addUser(0);
	board_->setLocalUser(0);

	// Get a local board editor
	delete toolbox_.editor();
	toolbox_.setEditor( board_->getEditor() );

	session_ = 0;
	emit parted();
	if(lock_) {
		lock_ = false;
		emit unlockboard();
	}
	sync_ = false;
	syncwait_ = false;
}

void Controller::addUser(int id)
{
	emit userJoined(session_->user(id));
}

void Controller::removeUser(int id)
{
	emit userParted(session_->user(id));
}

/**
 * Raster data download
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
			// TODO, downloaded invalid image
			Q_ASSERT(false);
		}
	}
	emit rasterDownloadProgress(p);
}

/**
 * Raster upload requested
 */
void Controller::rasterUpload()
{
	if(pendown_)
		sync_ = true;
	else
		sendRaster();
}

/**
 * Synchronization request.
 */
void Controller::syncWait()
{
	if(pendown_)
		syncwait_ = true;
	else
		lockForSync();
}

/**
 * Synchronization complete. Can start drawing again.
 */
void Controller::syncDone()
{
	emit unlockboard();
	lock_ = false;
	// Resend brush so the new client is up to date
	toolbox_.editor()->resendBrush();
}

/**
 * Session was locked ungracefully
 * @param lock lock or unlock
 */
void Controller::sessionLocked(bool lock)
{
	if(lock) {
		emit lockboard(tr("Locked by session owner"));
		if(pendown_ && tool_->readonly()==false) {
			penUp();
		}
		lock_ = true;
	} else {
		const network::User &localuser = session_->user(host_->localUser().id());
		// Unlock if local user is not locked and general session lock was lifted
		if(localuser.locked()==false && session_->isLocked()==false) {
			emit unlockboard();
			lock_ = false;
		}
	}
}

/**
 * User has been locked or unlocked
 * @param id user id
 */
void Controller::userLocked(int id, bool lock)
{
	const network::User &user = session_->user(id);

	// Session owner cannot be locked (except when entire board is locked)
	if(user.isOwner() && user.isLocal())
		return;

	qDebug() << "user locked" << id << lock;
	if(!user.isLocal())
		emit userChanged(user);

	if(lock) {
		// Lock UI if local user was locked
		if(user.isLocal() && lock_==false)
			sessionLocked(true);
	} else {
		// Unlock UI if local user was unlocked and general session lock
		// is not in place.
		if(user.isLocal() && session_->isLocked()==false)
			sessionLocked(false);
	}
}

/**
 * Handle session ownership change
 */
void Controller::sessionOwnerChanged()
{
	qDebug() << "owner changed to " << session_->info().owner;
	if(session_->info().owner == host_->localUser().id())
		emit becameOwner();
}

/**
 * User got kicked out
 * @param id id of the kicked user
 */
void Controller::sessionKicked(int id)
{
	emit userKicked(session_->user(id));
	if(session_->user(id).isLocal())
		disconnectHost();
}

/**
 * User limit of the session was changed
 * @param count max. number of users
 */
void Controller::sessionUserLimitChanged(int count)
{
	emit joinsDisallowed(count<2);
}

void Controller::sendRaster()
{
	Q_ASSERT(session_);
	QByteArray raster;
	QBuffer buffer(&raster);
	board_->image().save(&buffer, "PNG");
	session_->sendRaster(raster);
}

void Controller::lockForSync()
{
	emit lockboard(tr("Synchronizing new user"));
	lock_ = true;
	session_->sendAckSync();
}

void Controller::setTool(tools::Type tool)
{
	tool_ = toolbox_.get(tool);
}

void Controller::penDown(const drawingboard::Point& point)
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

void Controller::penMove(const drawingboard::Point& point)
{
	if(lock_ == false || lock_ && tool_->readonly()) {
		tool_->motion(point);
	}
}

void Controller::penUp()
{
	if(lock_ == false || lock_ && tool_->readonly()) {
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

void Controller::netConnected()
{
	host_->login(username_);
	emit connected(address_);
}

void Controller::netDisconnected(const QString& message)
{
	net_->wait();
	delete net_;
	net_ = 0;
	host_->setConnection(0);
	session_ = 0;
	emit disconnected(message);
}

