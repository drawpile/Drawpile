// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_MESSAGE_H
#define DP_NET_MESSAGE_H

#include <Qt>
#include <QMap>
#include <QString>
#include <QList>

namespace protocol {

/**
 * Drawpile network protocol message types
 */
enum MessageType {
	// Control messages (transparent)
	MSG_COMMAND=0,
	MSG_DISCONNECT,
	MSG_PING,

	// Reserved ID for internal use (not serializable)
	MSG_INTERNAL=31,

	// Meta messages (transparent)
	MSG_USER_JOIN=32,
	MSG_USER_LEAVE,
	MSG_SESSION_OWNER,
	MSG_CHAT,
	MSG_TRUSTED_USERS,
	MSG_SOFTRESET,
	MSG_PRIVATE_CHAT,

	// Meta messages (opaque)
	MSG_INTERVAL=64,
	MSG_LASERTRAIL,
	MSG_MOVEPOINTER,
	MSG_MARKER,
	MSG_USER_ACL,
	MSG_LAYER_ACL,
	MSG_FEATURE_LEVELS,
	MSG_LAYER_DEFAULT,
	MSG_FILTERED,
	MSG_EXTENSION, // reserved for non-standard extension use

	// Command messages (opaque)
	MSG_UNDOPOINT=128,
	MSG_CANVAS_RESIZE,
	MSG_LAYER_CREATE,
	MSG_LAYER_ATTR,
	MSG_LAYER_RETITLE,
	MSG_LAYER_ORDER,
	MSG_LAYER_DELETE,
	MSG_LAYER_VISIBILITY,
	MSG_PUTIMAGE,
	MSG_FILLRECT,
	MSG_TOOLCHANGE_REMOVED, // replaced by drawdabs*
	MSG_PEN_MOVE_REMOVED,   // replaced by drawdabs*
	MSG_PEN_UP,
	MSG_ANNOTATION_CREATE,
	MSG_ANNOTATION_RESHAPE,
	MSG_ANNOTATION_EDIT,
	MSG_ANNOTATION_DELETE,
	MSG_REGION_MOVE,
	MSG_PUTTILE,
	MSG_CANVAS_BACKGROUND,
	MSG_DRAWDABS_CLASSIC,
	MSG_DRAWDABS_PIXEL,
	MSG_DRAWDABS_PIXEL_SQUARE,
	MSG_UNDO=255,
};

enum MessageUndoState {
	DONE   = 0x00, /* done/not undone */
	UNDONE = 0x01, /* marked as undone, can be redone */
	GONE   = 0x03  /* marked as undone, cannot be redone */
};

// Note: both QHash and QMap work here, but we use QMap so the kwargs are
// ordered consistently, which makes comparing messages by eye easier.
typedef QMap<QString,QString> Kwargs;
typedef QMapIterator<QString,QString> KwargsIterator;

class MessagePtr;
class NullableMessageRef;

class Message {
	friend class MessagePtr;
	friend class NullableMessageRef;
public:
	//! Length of the fixed message header
	static const int HEADER_LEN = 4;

	Message(MessageType type, uint8_t ctx): m_type(type), _undone(DONE), m_refcount(0), m_contextid(ctx) {}
	Message(Message &other) = delete;
	Message(Message &&other) = delete;
	Message &operator=(Message &other) = delete;
	Message &operator=(Message &&other) = delete;
	virtual ~Message() = default;

	/**
	 * @brief Get the type of this message.
	 * @return message type
	 */
	MessageType type() const { return m_type; }

	/**
	 * @brief Is this a control message
	 *
	 * Control messages are used for things that are related to the server and not
	 * the session directly (e.g. setting server settings.)
	 */
	bool isControl() const { return m_type < 32; }

	/**
	 * @brief Is this a meta message?
	 *
	 * Meta messages are part of the session, but do not directly affect drawing.
	 * However, some meta message (those related to access controls) do affect
	 * how the command messages are filtered.
	 */
	bool isMeta() const { return m_type >= 31 && m_type < 128; }

	/**
	 * @brief Check if this message type is a command stream type
	 *
	 * Command stream messages are the messages directly related to drawing.
	 * The canvas can be reconstructed exactly using only command messages.
	 * @return true if this is a drawing command
	 */
	bool isCommand() const { return m_type >= 128; }

	/**
	 * @brief Is this an opaque message
	 *
	 * Opaque messages are those messages that the server does not need to understand and can
	 * merely pass along as binary data.
	 */
	bool isOpaque() const { return m_type >= 64; }

	/**
	 * @brief Is this a recordable message?
	 *
	 * All Meta and Command messages are recordable. Only the Control messages,
	 * which are used just for client/server communications, are ignored.
	 */
	bool isRecordable() const { return m_type >= 32; }

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
	uint8_t contextId() const { return m_contextid; }

	/**
	 * @brief Set the user ID of this message
	 *
	 * @param userid the new user id
	 */
	void setContextId(uint8_t userid) { m_contextid = userid; }

	/**
	 * @brief Get the ID of the layer this command affects
	 *
	 * For commands that do not affect any particular layer, 0 should
	 * be returned.
	 *
	 * Annotation editing commands can return the annotation ID here.
	 *
	 * @return layer (or equivalent) ID or 0 if not applicable
	 */
	virtual uint16_t layer() const { return 0; }

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
	 * @param decodeOpaque automatically decode opaque messages rather than returning OpaqueMessage
	 * @return message or 0 if type is unknown
	 */
	static NullableMessageRef deserialize(const uchar *data, int buflen, bool decodeOpaque);

	/**
	 * @brief Check if this message has the same content as the other one
	 * @param m
	 * @return
	 */
	bool equals(const Message &m) const;

	/**
	 * @brief Get the textmode serialization of this message
	 */
	virtual QString toString() const;

	//! Get the name of this message
	virtual QString messageName() const = 0;

	/**
	 * @brief Get a copy of this message wrapped in a Filtered message
	 *
	 * This is used when a message is filtered away, but we want to preserve
	 * the message for debugging reasons.
	 *
	 * @return a new Filtered instance
	 */
	MessagePtr asFiltered() const;

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

	/**
	 * @brief Check if the other message has identical payload
	 *
	 * The default implementation calls serializePayload and does a bytewise comparison
	 * on that. Subclasses should override this with a more efficient check.
	 *
	 * @param m
	 * @return true if payloads are equal
	 */
	virtual bool payloadEquals(const Message &m) const;

	/**
	 * @brief Get the keyword arguments that describe this message
	 *
	 * This is used by toString() to generate the textmode serialization.
	 */
	virtual Kwargs kwargs() const = 0;

private:
	const MessageType m_type;
	MessageUndoState _undone;
	int m_refcount;
	uint8_t m_contextid;
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
	class [[maybe_unused]] AbstractMessageMarker : Message {
		inline QString toString() const override { return QString(); }
	};
}

typedef QList<MessagePtr> MessageList;

/**
 * @brief Base class for messages without a payload
 */
template<class M> class ZeroLengthMessage : public Message {
public:
	ZeroLengthMessage(MessageType type, uint8_t ctx) : Message(type, ctx) { }

	static M *deserialize(uint8_t ctx, const uchar *data, int buflen) {
		Q_UNUSED(data);
		if(buflen!=0)
			return nullptr;
		return new M(ctx);
	}

	static M *fromText(uint8_t ctx, const Kwargs &) {
		return new M(ctx);
	}

protected:
	int payloadLength() const override { return 0; }
	int serializePayload(uchar *data) const override { Q_UNUSED(data); return 0; }
	bool payloadEquals(const Message &m) const override { Q_UNUSED(m); return true; }
	Kwargs kwargs() const override { return Kwargs(); }
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
		: d(msg)
	{
		Q_ASSERT(d);
		Q_ASSERT(d->m_refcount==0);
		++d->m_refcount;
	}

	MessagePtr(const MessagePtr &ptr) : d(ptr.d) { ++d->m_refcount; }

	static MessagePtr fromNullable(const NullableMessageRef &ref) { return MessagePtr(ref); }

	~MessagePtr()
	{
		Q_ASSERT(d->m_refcount>0);
		if(--d->m_refcount == 0)
			delete d;
	}

	MessagePtr &operator=(const MessagePtr &msg)
	{
		if(msg.d != d) {
			Q_ASSERT(d->m_refcount>0);
			if(--d->m_refcount == 0)
				delete d;
			d = msg.d;
			++d->m_refcount;
		}
		return *this;
	}

	Message &operator*() const { return *d; }
	Message *operator->() const { return d; }

	template<class msgtype> msgtype &cast() const { return static_cast<msgtype&>(*d); }

	inline bool equals(const MessagePtr &m) const { return d->equals(*m); }
	inline bool equals(const NullableMessageRef &m) const;

private:
	inline MessagePtr(const NullableMessageRef &ref);

	Message *d;
};

/**
* @brief A nullable reference counting pointer for Messages
*
* This object is the length of a normal pointer so it can be used
* efficiently with QList.
*
* @todo Maybe rename MessagePtr to MessageRef and this to MessagePtr?
*/
class NullableMessageRef {
public:
	NullableMessageRef() : d(nullptr) { }
	NullableMessageRef(std::nullptr_t np) : d(np) { }

	/**
	 * @brief Take ownership of the given raw Message pointer.
	 *
	 * The message will be deleted when reference count falls to zero.
	 * @param msg
	 */
	explicit NullableMessageRef(Message *msg)
		: d(msg)
	{
		if(d) {
			Q_ASSERT(d->m_refcount==0);
			++d->m_refcount;
		}
	}

	NullableMessageRef(const MessagePtr &ptr) : d(&(*ptr)) { ++d->m_refcount; }
	NullableMessageRef(const NullableMessageRef &ptr) : d(ptr.d) { if(d) ++d->m_refcount; }

	~NullableMessageRef()
	{
		if(d) {
			Q_ASSERT(d->m_refcount>0);
			if(--d->m_refcount == 0)
				delete d;
		}
	}

	NullableMessageRef &operator=(const NullableMessageRef &msg)
	{
		if(msg.d != d) {
			if(d) {
				Q_ASSERT(d->m_refcount>0);
				if(--d->m_refcount == 0)
					delete d;
			}
			d = msg.d;
			if(d)
				++d->m_refcount;
		}
		return *this;
	}

	NullableMessageRef &operator=(const MessagePtr &msg)
	{
		if(&(*msg) != d) {
			if(d) {
				Q_ASSERT(d->m_refcount>0);
				if(--d->m_refcount == 0)
					delete d;
			}
			d = &(*msg);
			++d->m_refcount;
		}
		return *this;
	}

	inline bool isNull() const { return !d; }

	Message &operator*() const { Q_ASSERT(d); return *d; }
	Message *operator->() const { Q_ASSERT(d); return d; }

	template<class msgtype> msgtype &cast() const { Q_ASSERT(d); return static_cast<msgtype&>(*d); }

	inline bool equals(const MessagePtr &m) const { return d && d->equals(*m); }
	inline bool equals(const NullableMessageRef &m) const { return d && m.d && d->equals(*m); }

private:
	Message *d;
};

MessagePtr::MessagePtr(const NullableMessageRef &ref)
	: d(&(*ref))
{
	if(!d)
		qFatal("MessagePtr::fromNullable(nullptr) called!");
	++d->m_refcount;
}

bool MessagePtr::equals(const NullableMessageRef &m) const { return !m.isNull() && d->equals(*m); }

}

Q_DECLARE_TYPEINFO(protocol::MessagePtr, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(protocol::NullableMessageRef, Q_MOVABLE_TYPE);

#endif
