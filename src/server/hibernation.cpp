/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "hibernation.h"
#include "../shared/server/session.h"
#include "../shared/util/logger.h"
#include "../shared/util/passwordhash.h"
#include "../shared/record/reader.h"
#include "../shared/record/writer.h"

#include <QDir>
#include <QRegularExpression>

namespace server {

Hibernation::Hibernation(const QString &sessionDirectory, QObject *parent)
	: SessionStore(parent), _path(sessionDirectory)
{
}

bool Hibernation::init()
{
	// Ensure storage directory exists
	QDir dir(_path);
	if(!dir.exists()) {
		if(!dir.mkpath(".")) {
			logger::error() << "Couldn't create session storage directory" << _path;
			return false;
		}
	}

	// Scan for sessions
	const QRegularExpression re("^session([.-])([a-zA-Z0-9:-]{1,64})\\.dphib(?:\\.(?:gz|bz2|xz))?$");

	for(const QString &filename : dir.entryList(QStringList() << "session*.dphib*", QDir::Files | QDir::Readable)) {
		QRegularExpressionMatch m = re.match(filename);
		if(!m.hasMatch())
			continue;

		recording::Reader reader(dir.filePath(filename));

		// Note. We use strict version mode, because if compatability mode is enabled, it will change
		// the recordings minor version number too.
		reader.setCompatabilityMode(false);

		recording::Compatibility comp = reader.open();

		if(!reader.isHibernation()) {
			logger::warning() << "Not a hibernated session:" << filename;
			continue;
		}

		if(comp != recording::COMPATIBLE && comp != recording::MINOR_INCOMPATIBILITY) {
			logger::warning() << "Incompatible hibernated session:" << filename;
			continue;
		}

		bool isCustomId = m.captured(1) == ".";

		SessionDescription desc;
		desc.id = SessionId(m.captured(2), isCustomId);
		desc.protoMinor = reader.hibernationHeader().minorVersion;
		desc.title = reader.hibernationHeader().title;
		desc.founder = reader.hibernationHeader().founder;
		desc.passwordHash = reader.hibernationHeader().password;
		desc.persistent = reader.hibernationHeader().flags & recording::HibernationHeader::PERSISTENT;
		desc.hibernating = true;
		desc.hibernationFile = reader.filename();

		_sessions.append(desc);
		logger::debug() << "Found hibernated session:" << filename;
	}

	logger::info() << "Found" << _sessions.size() << "hibernated session(s)";

	return true;
}

SessionDescription Hibernation::getSessionDescriptionById(const QString &id) const
{
	for(const SessionDescription &sd : _sessions)
		if(sd.id == id)
			return sd;
	return SessionDescription();
}

Session *Hibernation::takeSession(const QString &id)
{
	// Find and remove the session from the list
	QMutableListIterator<SessionDescription> it(_sessions);
	SessionDescription sd;
	while(it.hasNext()) {
		SessionDescription s = it.next();
		if(s.id.id() == id) {
			sd = s;
			it.remove();
			break;
		}
	}

	if(sd.id.isEmpty())
		return nullptr;

	// Load the session
	QString filename = sd.hibernationFile;
	recording::Reader reader(filename);
	if(reader.open() != recording::COMPATIBLE) {
		logger::error() << "Unable to open" << filename << "error:" << reader.errorString();
		return nullptr;
	}

	Session *session = new Session(sd.id, reader.hibernationHeader().minorVersion, reader.hibernationHeader().founder);

	// enable session persistence for now to get the flag right
	session->setPersistenceAllowed(true);

	// Restore settings not stored in the message stream
	session->setPasswordHash(sd.passwordHash);

	// Create initial snapshot point
#if 0 // TODO
	session->addSnapshotPoint();
	protocol::SnapshotPoint &snapshot = session->mainstream().snapshotPoint().cast<protocol::SnapshotPoint>();

	// Read snapshot
	int errors = 0;
	recording::MessageRecord msg;
	while((msg = reader.readNext()).status != recording::MessageRecord::END_OF_RECORDING) {
		if(msg.status == recording::MessageRecord::INVALID) {
			++errors;

		} else {
			snapshot.append(protocol::MessagePtr(msg.message));
		}
	}

	snapshot.append(protocol::MessagePtr(new protocol::SnapshotMode(0, protocol::SnapshotMode::END)));

	// Bring session state up to speed with the content
	session->syncInitialState(snapshot.substream());

	logger::debug() << "loaded" << snapshot.substream().size() << "messages from" << filename;

	if(errors>0)
		logger::warning() << errors << "invalid message(s) were skipped while de-hibernating" << filename;
#endif

	reader.close();

	// Session has been woken up, so delete the hibernation file.
	QFile(filename).remove();

	return session;
}

bool Hibernation::storeSession(const Session *session)
{
	QString filename = QDir(_path).filePath(QString("session%1%2.dphib.gz").arg(session->id().isCustom() ? "." : "-", session->id()));

	recording::Writer writer(filename);
	writer.setFilterMeta(false);
	writer.setMinimumInterval(0);

	if(!writer.open()) {
		logger::error() << "Couldn't open" << filename << "for writing:" << writer.errorString();
		return false;
	}

	// Write header
	recording::HibernationHeader header;
	header.minorVersion = session->minorProtocolVersion();
	header.title = session->title();
	header.founder = session->founder();
	header.password = session->passwordHash();

	if(session->isPersistent())
		header.flags |= recording::HibernationHeader::PERSISTENT;

	writer.writeHibernationHeader(header);

	// Write session
	const protocol::MessageStream &msgs = session->mainstream();

	int idx = msgs.offset();
	while(msgs.isValidIndex(idx)) {
		writer.recordMessage(msgs.at(idx));
		++idx;
	}

	writer.close();

	SessionDescription desc(*session);
	desc.hibernationFile = filename;
	_sessions.append(desc);
	emit sessionAvailable(desc);

	return true;
}

bool Hibernation::deleteSession(const QString &id)
{
	// Find and remove the session from the list
	QMutableListIterator<SessionDescription> it(_sessions);
	SessionDescription sd;
	while(it.hasNext()) {
		SessionDescription s = it.next();
		if(s.id == id) {
			sd = s;
			it.remove();
			break;
		}
	}

	// not found
	if(sd.id==0)
		return false;

	// Remove from filesystem
	return QFile(sd.hibernationFile).remove();
}

}
