/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#ifndef DP_NET_MESSAGE_H
#define DP_NET_MESSAGE_H

#include <Qt>

namespace protocol {

/**
 * Drawpile network protocol message types
 */
enum MessageType {
	// Login stream
	MSG_LOGIN,

	// Meta stream
	MSG_USER_JOIN,
	MSG_USER_ATTR,
	MSG_USER_LEAVE,
	MSG_CHAT,
	MSG_LAYER_ACL,
	MSG_SNAPSHOT,
	MSG_SNAPSHOTPOINT,
	MSG_SESSION_TITLE,
	MSG_SESSION_CONFIG,
	MSG_STREAMPOS,
	MSG_INTERVAL,
	MSG_MOVEPOINTER,
	MSG_MARKER,
	MSG_DISCONNECT,
	MSG_PING,

	// Command stream
	MSG_CANVAS_RESIZE=128,
	MSG_LAYER_CREATE,
	MSG_LAYER_COPY,
	MSG_LAYER_ATTR,
	MSG_LAYER_RETITLE,
	MSG_LAYER_ORDER,
	MSG_LAYER_DELETE,
	MSG_PUTIMAGE,
	MSG_TOOLCHANGE,
	MSG_PEN_MOVE,
	MSG_PEN_UP,
	MSG_ANNOTATION_CREATE,
	MSG_ANNOTATION_RESHAPE,
	MSG_ANNOTATION_EDIT,
	MSG_ANNOTATION_DELETE,
	MSG_UNDOPOINT,
	MSG_UNDO,
	MSG_FILLRECT
};

enum MessageUndoState {
	DONE   = 0x00, /* done/not undone */
	UNDONE = 0x01, /* marked as undone, can be redone */
	GONE   = 0x03  /* marked as undone, cannot be redone */
};

class Message {
	friend class MessagePtr;
public:
	//! Length of the fixed message header
	static const int HEADER_LEN = 3;

	Message(MessageType type, uint8_t ctx): _type(type), _undone(DONE), _refcount(0), _contextid(ctx) {}
	virtual ~Message() {};
	
	/**
	 * @brief Get the type of this message.
	 * @return message type
	 */
	MessageType type() const { return _type; }
	
	/**
	 * @brief Check if this message type is a command stream type
	 * 
	 * Command stream messages are the messages directly related to drawing.
	 * The canvas can be reconstructed exactly using only command messages.
	 * @return true if this is a drawing command
	 */
	bool isCommand() const { return _type >= MSG_CANVAS_RESIZE; }

	/**
	 * @brief Get the message length, header included
	 * @return message length in bytes
	 */
	int length() const { return HEADER_LEN + payloadLength(); }

	/**
	 * @brief Get the user context ID of this message
	 *
	 * The ID is 0 for messages that are not related to any user
	 * @return context ID or 0 if not applicable
	 */
	uint8_t contextId() const { return _contextid; }

	/**
	 * @brief Set the user ID of this message
	 *
	 * @param userid the new user id
	 */
	void setContextId(uint8_t userid) { _contextid = userid; }

	/**
	 * @brief Does this command need operator privileges to issue?
	 * @return true if user must be session operator to send this
	 */
	virtual bool isOpCommand() const { return false; }

	/**
	 * @brief Is this message type undoable?
	 *
	 * By default, all Command messages are undoable.
	 *
	 * @return true if this action can be undone
	 */
	virtual bool isUndoable() const { return isCommand(); }

	/**
	 * @brief Has this command been marked as undone?
	 *
	 * Note. This is a purely local flag that is not part of the
	 * protocol. It is here to avoid the need to maintain an
	 * external undone action list.
	 *
	 * @return true if this message has been marked as undone
	 */
	MessageUndoState undoState() const { return _undone; }

	/**
	 * @brief Mark this message as undone
	 *
	 * Note. Not all messages are undoable. This function
	 * does nothing if this message type doesn't support undoing.
	 *
	 * @param undone new undo flag state
	 */
	void setUndoState(MessageUndoState undo) { if(isUndoable()) _undone = undo; }

	/**
	 * @brief Serialize this message
	 *
	 * The data buffer must be long enough to hold length() bytes.
	 * @param data buffer where to write the message
	 * @return number of bytes written (should always be length())
	 */
	int serialize(char *data) const;

	/**
	 * @brief get the length of the message from the given data
	 *
	 * Data buffer should be at least two bytes long
	 * @param data data buffer
	 * @return length
	 */
	static int sniffLength(const char *data);

	/**
	 * @brief deserialize a message from data buffer
	 *
	 * The provided buffer should contain at least sniffLength(data)
	 * bytes. The parameter buflen is the maximum length of the buffer.
	 * If the announced length of the message is less than the buffer
	 * length, a null pointer is returned.
	 *
	 * If the message type is unrecognized or the message content is
	 * determined to be invalid, a null pointer is returned.
	 *
	 * @param data input data buffer
	 * @param buflen length of the data buffer
	 * @return message or 0 if type is unknown
	 */
	static Message *deserialize(const uchar *data, int buflen);

protected:
	/**
	 * @brief Get the length of the message payload
	 * @return payload length in bytes
	 */
	virtual int payloadLength() const = 0;

	/**
	 * @brief Serialize the message payload
	 * @param data data buffer
	 * @return number of bytes written (should always be the same as payloadLenth())
	 */
	virtual int serializePayload(uchar *data) const = 0;

private:
	const MessageType _type;
	MessageUndoState _undone;
	int _refcount;
	uint8_t _contextid; // this is part of the payload for those message types that have it
};

/**
* @brief A reference counting pointer for Messages
*
* This object is the length of a normal pointer so it can be used
* efficiently with QList.
*
* @todo use QAtomicInt if thread safety is needed
*/
class MessagePtr {
public:
	/**
	 * @brief Take ownership of the given raw Message pointer.
	 *
	 * The message will be deleted when reference count falls to zero.
	 * Null pointers are not allowed.
	 * @param msg
	 */
	explicit MessagePtr(Message *msg)
		: _ptr(msg)
	{
		Q_ASSERT(_ptr);
		Q_ASSERT(_ptr->_refcount==0);
		++_ptr->_refcount;
	}

	MessagePtr(const MessagePtr &ptr) : _ptr(ptr._ptr) { ++_ptr->_refcount; }

	~MessagePtr()
	{
		Q_ASSERT(_ptr->_refcount>0);
		if(--_ptr->_refcount == 0)
			delete _ptr;
	}

	MessagePtr &operator=(const MessagePtr &msg)
	{
		if(msg._ptr != _ptr) {
			Q_ASSERT(_ptr->_refcount>0);
			if(--_ptr->_refcount == 0)
				delete _ptr;
			_ptr = msg._ptr;
			++_ptr->_refcount;
		}
		return *this;
	}

	Message &operator*() const { return *_ptr; }
	Message *operator ->() const { return _ptr; }

	template<class msgtype> msgtype &cast() const { return static_cast<msgtype&>(*_ptr); }

private:
	Message *_ptr;
};

}

Q_DECLARE_TYPEINFO(protocol::MessagePtr, Q_MOVABLE_TYPE);

#endif
