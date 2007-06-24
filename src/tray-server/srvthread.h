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

#ifndef ServerThread_INCLUDED
#define ServerThread_INCLUDED

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "../server/server.h"

class Server;

class ServerThread
	: public QThread
{
	Q_OBJECT
	
public:
	ServerThread(Server *srv, QObject *parent=0);
	~ServerThread();
	
protected:
	void run();
	
	Server *srv;
	
private:
	QMutex mutex;
	QWaitCondition condition;
	
	bool restart;
	bool abort;
};

#endif // ServerThread_INCLUDED
