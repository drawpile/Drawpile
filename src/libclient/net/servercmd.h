/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "libclient/drawdance/message.h"

namespace net {

/**
 * @brief A command sent to the server using the (Control) Command message
 */
struct ServerCommand {
	QString cmd;
	QJsonArray args;
	QJsonObject kwargs;

	drawdance::Message toMessage() const;

	// Convenience functions_
	static drawdance::Message make(const QString &cmd, const QJsonArray &args=QJsonArray(), const QJsonObject &kwargs=QJsonObject());

	//! Kick (and optionally ban) a user from the session
	static drawdance::Message makeKick(int target, bool ban);

	//! Remove a ban entry
	static drawdance::Message makeUnban(int entryId);

	//! (Un)mute a user
	static drawdance::Message makeMute(int target, bool mute);

	//! Request the server to announce this session at a listing server
	static drawdance::Message makeAnnounce(const QString &url, bool privateMode);

	//! Request the server to remove an announcement at the listing server
	static drawdance::Message makeUnannounce(const QString &url);
};

/**
 * @brief A reply or notification from the server received with the Command message
 */
struct ServerReply {
	enum class ReplyType {
		Unknown,
		Login,   // used during the login phase
		Message, // general chat type notifcation message
		Alert,   // urgen notification message
		Error,   // error occurred
		Result,  // comand result
		Log,     // server log message
		SessionConf, // session configuration update
		SizeLimitWarning, // session history size nearing limit (deprecated)
		Status,  // Periodic status update
		Reset,   // session reset state
		Catchup,  // number of messages queued for upload (use for progress bars)
		ResetRequest, // request client to perform a reset
	} type;
	QString message;
	QJsonObject reply;

	static ServerReply fromMessage(const drawdance::Message &msg);
	static ServerReply fromJson(const QJsonDocument &doc);
	QJsonDocument toJson() const;
};

}
