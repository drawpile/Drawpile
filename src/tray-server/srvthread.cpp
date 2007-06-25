/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

******************************************************************************/

#include "srvthread.h"

#include <QDebug>

ServerThread::ServerThread(Server *server, QObject *parent)
	: QThread(parent),
	srv(server)
{
	Q_ASSERT(server != 0);
	
	if (!srv->init())
	{
		qDebug() << "Server initialization failed!";
		throw std::exception();
	}
}

ServerThread::~ServerThread()
{
	stop();
	wait();
}

void ServerThread::run()
{
	srv->run();
	srv->reset();
}

void ServerThread::stop()
{
	srv->stop();
}
