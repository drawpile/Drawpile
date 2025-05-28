// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_SERVER_BUILTINRESET_H
#define LIBCLIENT_SERVER_BUILTINRESET_H
#include "libshared/net/message.h"
#include <QObject>

namespace drawdance {
class AclState;
class CanvasState;
}

namespace server {

class BuiltinReset final : public QObject {
	Q_OBJECT
public:
	BuiltinReset(net::MessageList &&initialMessages)
		: m_resetImage(initialMessages)
	{
	}

	void appendResetImageMessage(net::Message &&msg)
	{
		m_resetImage.append(msg);
	}

	void appendPostResetMessage(const net::Message &msg)
	{
		m_postResetMessages.append(msg);
	}

	void appendPostResetMessage(net::Message &&msg)
	{
		m_postResetMessages.append(msg);
	}

	void appendAclsToPostResetMessages(
		const drawdance::AclState &acls, uint8_t localUserId,
		bool compatibilityMode);

	void generateResetImage(
		const drawdance::CanvasState &canvasState, bool compatibilityMode);

	net::MessageList takeResetImage();

signals:
	void resetImageGenerated();

private:
	net::MessageList m_resetImage;
	net::MessageList m_postResetMessages;
};

}

#endif
