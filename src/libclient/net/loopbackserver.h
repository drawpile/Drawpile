/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#ifndef DP_NET_LOOPBACKSERVER_H
#define DP_NET_LOOPBACKSERVER_H

#include "server.h"

#include <QSslCertificate>

namespace net {

/**
 * \brief Loopback sever for local single user mode
 */
class LoopbackServer : public Server
{
	Q_OBJECT
public:
	explicit LoopbackServer(QObject *parent=nullptr);
	
	void sendMessage(const protocol::MessagePtr &msg) override;
	void sendMessages(const protocol::MessageList &msg) override;
	void sendEnvelope(const Envelope &e) override;
	void logout() override;

	bool isLoggedIn() const override { return false; }
	int uploadQueueBytes() const override { return 0; }
	Security securityLevel() const override { return NO_SECURITY; }
	QSslCertificate hostCertificate() const override { return QSslCertificate(); }
	bool supportsPersistence() const override { return false; }
	bool supportsAbuseReports() const override { return false; }

};


}

#endif

