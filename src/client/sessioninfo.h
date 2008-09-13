/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#ifndef SESSIONINFO_H
#define SESSIONINFO_H

#include <QString>
#include <QList>

namespace protocol {
	class SessionInfo;
}

namespace network {

class SessionState;

//! Information about a session
class Session {
	public:
		//! Get session info from a board message
		Session(const QStringList& info);
		Session(int o, const QString& title, int width, int height, int maxusers, bool deflock);
		QStringList tokens() const;

		int owner() const { return owner_; }
		const QString& title() const { return title_; }
		int width() const { return width_; }
		int height() const { return height_; }
		int maxUsers() const { return maxusers_; }
		bool lock() const { return lock_; }
		bool defLock() const { return deflock_; }

	private:
		int owner_;
		QString title_;
		quint16 width_;
		quint16 height_;
		int maxusers_;
		bool lock_;
		bool deflock_;
};

//! Session participant
class User {
	public:
		User();
		User(const QString& name, int id, bool locked, SessionState *owner);
		User(SessionState *owner, const QStringList& tokens);

		//! Get the name of the user
		const QString& name() const { return name_; }

		//! Get the ID of the user
		int id() const { return id_; }

		//! Is the user locked
		bool locked() const { return locked_; }

		//! Set lock status
		void setLocked(bool lock);

		//! Is the user the local user
		bool isLocal() const;

		//! Is the user the session owner
		bool isOwner() const;

		//! Lock or unlock the user
		void lock(bool l);

		//! Kick the user out of the session
		void kick();

	private:
		QString name_;
		int id_;
		bool locked_;
		SessionState *owner_;
};

}

#endif

