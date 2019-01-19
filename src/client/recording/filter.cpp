/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "filter.h"

#include "../shared/record/reader.h"
#include "../shared/record/writer.h"
#include "../shared/net/undo.h"
#include "../shared/net/brushes.h"
#include "../shared/net/recording.h"

#include <QDebug>
#include <QVector>
#include <QHash>
#include <QSet>

namespace recording {

namespace {

static const uchar UNDOABLE = (1<<7);
static const uchar REMOVED = (1<<6);

struct FilterIndex {
	// message type (protocol)
	uchar type;

	// message context ID
	uchar ctxid;

	// Filtering flags
	uchar flags;

	// Message position in the recording file
	qint64 offset;
};

struct State {
	// A FilterIndex entry for each message in the recording
	QVector<FilterIndex> index;

	// User join/leave message indices
	// (used for removing looky-loos)
	QHash<int, QList<int>> userjoins;

	// Replacement messages for specific indices
	// (currently used just for squishing)
	QHash<int, protocol::NullableMessageRef> replacements;

	// List of users who have participated in the session somehow
	// (other than just loggin in/out)
	QSet<int> users_seen;
};

static inline void mark_delete(FilterIndex &i) {
	i.flags |= REMOVED;
}

static inline protocol::MessageUndoState undostate(const FilterIndex &i) {
	return protocol::MessageUndoState(i.flags & 0x03);
}

static inline bool isUndoable(const FilterIndex &i) {
	return i.flags & UNDOABLE;
}

static inline bool isDeleted(const FilterIndex &i) {
	return i.flags & (REMOVED | protocol::UNDONE);
}

// Basic filtering (including Undo)
static void filterMessage(const FilterOptions &options, State &state, protocol::MessagePtr msg, qint64 offset)
{
	// Put this message in the index
	state.index.append(FilterIndex {
		uchar(msg->type()),
		msg->contextId(),
		msg->isUndoable() ? UNDOABLE : uchar(0),
		offset
	});

	// Filter out select message types
	switch(msg->type()) {
	using namespace protocol;
	case MSG_CHAT:
		if(options.removeChat) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_USER_JOIN:
	case MSG_USER_LEAVE:
		state.userjoins[msg->contextId()].append(state.index.size()-1);
		return;

	case MSG_INTERVAL:
		if(options.removeDelays) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_LASERTRAIL:
	case MSG_MOVEPOINTER:
		if(options.removeLasers) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_MARKER:
		if(options.removeMarkers) {
			mark_delete(state.index.last());
			return;
		}
		break;

	case MSG_FILTERED:
		// Always filter out FILTERED messages
		mark_delete(state.index.last());
		return;

	default: break;
	}

	// Perform undo
	if(msg->type() == protocol::MSG_UNDO && options.removeUndone) {
		// Normally, performing an undo will implicitly delete the undo action itself,
		// but it can be restored by a redo. Therefore, we must explicitly flag the
		// undo messages for deletion to be sure they are gone.
		mark_delete(state.index.last());

		// Perform an undo. This is a stripped down version of handleUndo from StateTracker
		const protocol::Undo &cmd = msg.cast<protocol::Undo>();

		const uchar ctxid = cmd.contextId();

		// Step 1. Find undo or redo point
		int pos = state.index.size();
		int upCount = 0;

		if(cmd.isRedo()) {
			// Find the start of the undo sequence (oldest undone UndoPoint)
			int redostart = pos;
			while(--pos>=0 && upCount <= protocol::UNDO_DEPTH_LIMIT) {
				const FilterIndex u = state.index[pos];
				if(u.type == protocol::MSG_UNDOPOINT) {
					++upCount;
					if(u.ctxid == ctxid) {
						if(undostate(u) != protocol::DONE)
							redostart = pos;
						else
							break;
					}
				}
			}

			if(redostart == state.index.size()) {
				qDebug() << "nothing to redo for user" << cmd.contextId();
				mark_delete(state.index.last());
				return;
			}
			pos = redostart;

		} else {
			// Search for undoable actions from the end of the
			// command stream towards the beginning
			while(--pos>=0 && upCount <= protocol::UNDO_DEPTH_LIMIT) {
				const FilterIndex u = state.index[pos];

				if(u.type == protocol::MSG_UNDOPOINT) {
					++upCount;
					if(u.ctxid == ctxid && undostate(u) == protocol::DONE)
						break;
				}
			}
		}

		if(upCount > protocol::UNDO_DEPTH_LIMIT) {
			qDebug() << "user" << cmd.contextId() << "cannot undo/redo beyond history limit";
			mark_delete(state.index.last());
			return;
		}

		// Step 2 is not needed here

		// Step 3. (Un)mark all actions by the user as undone
		if(cmd.isRedo()) {
			int i=pos;
			int sequence=2;
			while(i<state.index.size()) {
				FilterIndex &u = state.index[i];
				if(u.ctxid == ctxid) {
					if(u.type == protocol::MSG_UNDOPOINT && undostate(u) != protocol::GONE)
						if(--sequence==0)
							break;

					// GONE messages cannot be redone
					if(undostate(u) == protocol::UNDONE)
						u.flags &= ~protocol::UNDONE;
				}
				++i;
			}
		} else {
			for(int i=pos;i<state.index.size();++i) {
				FilterIndex &u = state.index[i];
				if(u.ctxid == ctxid && isUndoable(u))
					u.flags |= protocol::UNDONE;
			}
		}
		// Steps 4 is not needed here.
	}

	if(msg->contextId()>0)
		state.users_seen.insert(msg->contextId());

	return;
}

// Remove Join/Leave messages of those users who didn't do anything
// TODO this could be smarter. A user can log in&out many times, but
// not draw something every time. Perhaps this could preseve the
// outermost login/logout messages in that case?
static void filterLookyloos(State &state)
{
	for(const int id : state.userjoins.keys()) {
		if(!state.users_seen.contains(id)) {
			for(const int idx : state.userjoins[id]) {
				mark_delete(state.index[idx]);
			}
		}
	}
}

/**
 * @brief Remove adjacent undo points by the same user
 *
 * This removes all duplicate undo points that appear _next to each other_ in
 * the message stream. Such undo points are left over when actions are undone or
 * silenced.
 *
 * @param state
 */
static void filterAdjacentUndoPoints(State &state)
{
	FilterIndex prev {0, 0, 0, 0};

	for(int i=0;i<state.index.size();++i) {
		FilterIndex &fi = state.index[i];

		if(isDeleted(fi) || fi.type == protocol::MSG_INTERVAL)
			continue;

		if(
			fi.type == protocol::MSG_UNDOPOINT && prev.type == protocol::MSG_UNDOPOINT &&
			fi.ctxid == prev.ctxid
		) {
			mark_delete(fi);
		}

		prev = fi;
	}
}

static inline bool isDabMessage(uchar msgtype) {
	return msgtype == protocol::MSG_DRAWDABS_CLASSIC ||
		msgtype == protocol::MSG_DRAWDABS_PIXEL;
}

static bool squishStrokes(State &state, Reader &recording, QString *errorMessage)
{
	protocol::NullableMessageRef sequenceStartMessage;
	FilterIndex sequenceStart {0, 0, 0, 0};
	int sequenceStartIndex = 0;

	for(int i=0;i<state.index.size();++i) {
		FilterIndex &fi = state.index[i];

		if(isDeleted(fi))
			continue;

		// Encountering any non-dab message ends a squishable sequence
		if(!isDabMessage(fi.type)) {
			sequenceStartMessage = nullptr;
			continue;
		}

		// If we have an open sequence already, the new message type
		// and context ID must match
		if(!sequenceStartMessage.isNull()) {
			if(fi.type != sequenceStart.type || fi.ctxid != sequenceStart.ctxid) {
				sequenceStartMessage = nullptr;
				continue;
			}
		}

		// At this point this message is either the start of a new sequence
		// OR a potential continuation of the current one

		recording.seekTo(i, fi.offset);
		MessageRecord msgrecord = recording.readNext();
		if(msgrecord.status != MessageRecord::OK) {
			qWarning("Error reading message #%d at offset %lld", i, fi.offset);
			if(errorMessage)
				*errorMessage = "File read error";
			return false;
		}

		if(!isDabMessage(msgrecord.message->type())) {
			qWarning("BUG: Inconsistency when reading recording. Expected a dab message at offset %lld, got %s", fi.offset, qPrintable(msgrecord.message->messageName()));
			if(errorMessage)
				*errorMessage = "Internal Application Error in squishStrokes function";
			return false;
		}

		// If we have an open sequence, try squishing.
		if(!sequenceStartMessage.isNull() && sequenceStartMessage.cast<protocol::DrawDabs>().extend(msgrecord.message.cast<protocol::DrawDabs>())) {

			// Squish succeeded. Store the extended message.
			if(!state.replacements.contains(sequenceStartIndex))
				state.replacements[sequenceStartIndex] = sequenceStartMessage;

			mark_delete(fi);

		} else {
			// Did not squish. This is a start of a new potentially squishable sequence
			sequenceStart = fi;
			sequenceStartMessage = msgrecord.message;
			sequenceStartIndex = i;
		}
	}

	return true;
}

static bool doFilterRecording(const FilterOptions &options, State &state, Reader &recording, QString *errorMessage)
{
	// Read input file and perform basic filtering
	// This constructs the filtering index
	while(true) {
		MessageRecord msg = recording.readNext();
		if(msg.status == MessageRecord::END_OF_RECORDING)
			break;
		if(msg.status == MessageRecord::INVALID) {
			qWarning() << "skipping invalid message type" << msg.invalid_type;
			continue;
		}

		filterMessage(options, state, protocol::MessagePtr::fromNullable(msg.message), recording.currentPosition());
	}

	// More complicated filtering that uses the index

	if(options.removeLookyLoos)
		filterLookyloos(state);

	if(options.squishStrokes) {
		if(!squishStrokes(state, recording, errorMessage))
			return false;
	}

	filterAdjacentUndoPoints(state);

	return true;
}

}

bool filterRecording(const QString &input, const QString &outputfile, const FilterOptions &options, QString *errorMessage)
{
	// Step 1. Open input and output files
	Reader reader(input);
	Compatibility readOk = reader.open();
	if(readOk != COMPATIBLE && readOk != MINOR_INCOMPATIBILITY) {
		qWarning() << "Cannot open recording for filtering. Error code" << readOk;
		if(errorMessage)
			*errorMessage = reader.errorString();
		return false;
	}

	Writer writer(outputfile);
	if(!writer.open()) {
		qWarning() << "Cannot open record filter output file";
		if(errorMessage)
			*errorMessage = writer.errorString();
		return false;
	}

	// Step 2. Mark messages for removal and replacement
	State state;
	if(!doFilterRecording(options, state, reader, errorMessage))
		return false;

	// Step 3. Copy remaining messages to output file
	reader.rewind();

	writer.writeHeader();

	QByteArray buffer;
	while(reader.readNextToBuffer(buffer)) {
		const int pos = reader.currentIndex();

		// Copy (or replace) original message, unless marked for deletion
		if(!isDeleted(state.index[pos])) {

			if(state.replacements.contains(pos))
				writer.writeMessage(*state.replacements[pos]);
			else
				writer.writeFromBuffer(buffer);
		}
	}

	writer.close();

	return true;
}

}
