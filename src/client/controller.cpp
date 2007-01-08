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
#include "netstate.h"
#include "localserver.h"

#include "../shared/protocol.defaults.h"

Controller::Controller(QObject *parent)
	: QObject(parent), board_(0), editor_(0), net_(0), session_(0), pendown_(false), sync_(false), syncwait_(false), lock_(false)
{
	netstate_ = new network::HostState(this);
	connect(netstate_, SIGNAL(loggedin()), this, SLOT(serverLoggedin()));
	connect(netstate_, SIGNAL(joined(int)), this, SLOT(sessionJoined(int)));
	connect(netstate_, SIGNAL(parted(int)), this, SLOT(sessionParted()));

	connect(netstate_, SIGNAL(noSessions()), this, SLOT(disconnectHost()));
	connect(netstate_, SIGNAL(noSessions()), this, SIGNAL(noSessions()));
	connect(netstate_, SIGNAL(sessionNotFound()), this, SLOT(disconnectHost()));
	connect(netstate_, SIGNAL(sessionNotFound()), this, SIGNAL(sessionNotFound()));

	connect(netstate_, SIGNAL(selectSession(network::SessionList)), this, SIGNAL(selectSession(network::SessionList)));
	connect(netstate_, SIGNAL(needPassword()), this, SIGNAL(needPassword()));

	connect(netstate_, SIGNAL(error(QString)), this, SIGNAL(netError(QString)));
	// Disconnect on error
	connect(netstate_, SIGNAL(error(QString)), this, SLOT(disconnectHost()));
}

Controller::~Controller()
{
}

void Controller::setModel(drawingboard::Board *board)
{
	board_ = board;
	board_->addUser(0);
	board_->setLocalUser(0);
	editor_ = board->getEditor();
	tools::Tool::setEditor(editor_);
}

/**
 * Establish a connection with a server.
 * The connected signal will be emitted when connection is established
 * and loggedin when login is succesful.
 * If a path is specified, the session with a matching name is automatically
 * joined.
 * @param url host url. Should contain an username. May contain a path
 */
void Controller::connectHost(const QUrl& url)
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
	connect(net_,SIGNAL(received()), netstate_, SLOT(receiveMessage()));

	// Autojoin if path is present
	autojoinpath_ = url.path();

	// Connect to host
	netstate_->setConnection(net_);
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
 * @pre host connection must be established and user logged in.
 */
void Controller::hostSession(const QString& title, const QString& password,
		const QImage& image)
{
	netstate_->host(title, password, image.width(), image.height());
}

/**
 * If there is only one session, it is joined automatically. Otherwise a
 * list of sessions is presented to the user to choose from.
 */
void Controller::joinSession()
{
	netstate_->join();
}

void Controller::sendPassword(const QString& password)
{
	netstate_->sendPassword(password);
}

void Controller::joinSession(int id)
{
	netstate_->join(id);
}

void Controller::disconnectHost()
{
	Q_ASSERT(net_);
	net_->disconnectHost();
}

void Controller::serverLoggedin()
{
	emit loggedin();
	if(autojoinpath_.length()>1)
		netstate_->join(autojoinpath_.mid(1));
}

/**
 * A session was joined
 */
void Controller::sessionJoined(int id)
{
	session_ = netstate_->session(id);

	// Update user list
	board_->clearUsers();
	foreach(const network::User& ui, session_->users())
		board_->addUser(ui.id);
	board_->addUser(netstate_->localUserId());
	board_->setLocalUser(netstate_->localUserId());

	// Make session <-> controller connections
	connect(session_, SIGNAL(rasterReceived(int)), this, SLOT(rasterDownload(int)));
	connect(session_, SIGNAL(rasterSent(int)), this, SIGNAL(rasterUploadProgress(int)));
	connect(session_, SIGNAL(syncRequest()), this, SLOT(rasterUpload()));
	connect(session_, SIGNAL(syncWait()), this, SLOT(syncWait()));
	connect(session_, SIGNAL(syncDone()), this, SLOT(syncDone()));

	// Make session -> board connections
	connect(session_, SIGNAL(toolReceived(int,drawingboard::Brush)), board_, SLOT(userSetTool(int,drawingboard::Brush)));
	connect(session_, SIGNAL(strokeReceived(int,drawingboard::Point)), board_, SLOT(userStroke(int,drawingboard::Point)));
	connect(session_, SIGNAL(strokeEndReceived(int)), board_, SLOT(userEndStroke(int)));
	connect(session_, SIGNAL(userJoined(int)), board_, SLOT(addUser(int)));
	connect(session_, SIGNAL(userLeft(int)), board_, SLOT(removeUser(int)));


	// Get a remote board editor
	delete editor_;
	editor_ = board_->getEditor(session_);
	tools::Tool::setEditor(editor_);

	emit joined(session_->info().title);
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
	board_->clearPreviews();

	// Get a local board editor
	delete editor_;
	editor_ = board_->getEditor();
	tools::Tool::setEditor(editor_);

	session_ = 0;
	emit parted();
	if(lock_) {
		lock_ = false;
		emit unlockboard();
	}
	sync_ = false;
	syncwait_ = false;
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
	editor_->resendBrush();
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
	tool_ = tools::Tool::get(tool);
}

void Controller::penDown(const drawingboard::Point& point, bool isEraser)
{
	if(lock_ == false || lock_ && tool_->readonly()) {
		tool_->begin(point);
		if(tool_->readonly()==false) {
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
	netstate_->login(username_);
	emit connected(address_);
}

void Controller::netDisconnected(const QString& message)
{
	net_->wait();
	delete net_;
	net_ = 0;
	netstate_->setConnection(0);
	session_ = 0;
	emit disconnected(message);
}

