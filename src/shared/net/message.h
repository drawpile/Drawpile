#ifndef DP_PROTO_MESSAGE_H
#define DP_PROTO_MESSAGE_H

#include <QString>
#include <QStringList>

class QIODevice;

#include "protocol.h"

namespace protocol {

/**
 * A text message.
 * The format of the message is:
 * <COMMAND> [PARAM1] [PARAM2...]
 * If a parameter contains spaces, it should be quoted with ""
 * A " character can be escaped with \.
 * For example:
 * SAY "hello world"
 * SAY "hello \"world\""
 * SAY "Backslash: \\"
 */
class Message : public Packet {
	public:
		/**
		 * Construct a text message
		 * @param message message text.
		 */
		Message(const QString& message) :
			Packet(MESSAGE), _message(message) { }

		/**
		 * Construct a text message by properly quoting
		 * a list of tokens.
		 * @param tokens list of message tokens
		 */
		Message(const QStringList& tokens) :
			Packet(MESSAGE), _message(quote(tokens)) { }

		/**
		 * Deserialize a message
		 */
		static Message *deserialize(QIODevice& data, int len);

		/**
		 * Get the message.
		 */
		const QString& message() const { return _message; }

		/**
		 * Get the message broken into tokens by whitespace according to
		 * the quoting rules.
		 * @return list of string tokens.
		 */
		QStringList tokens() const;

		/**
		 * Construct a properly quoted message string from the tokens.
		 * @param tokens list of tokens.
		 * @return quoted message string
		 */
		static QString quote(const QStringList& tokens);

		unsigned int payloadLength() const;

	protected:
		void serializeBody(QIODevice& data) const;

	private:
		const QString _message;
};

}
#endif

