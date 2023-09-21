// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/server/builtinclient.h"

namespace server {

BuiltinClient::BuiltinClient(
	QTcpSocket *socket, ServerLog *logger, QObject *parent)
	: Client{socket, logger, true, parent}
{
}

BuiltinClient::~BuiltinClient()
{
	emit builtinClientDestroyed(this);
}

}
