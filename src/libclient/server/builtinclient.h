// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_SERVER_BUILTINCLIENT_H
#define LIBCLIENT_SERVER_BUILTINCLIENT_H
#include "libserver/client.h"

namespace server {

class BuiltinClient final : public Client {
	Q_OBJECT
public:
	BuiltinClient(
		QTcpSocket *socket, ServerLog *logger, QObject *parent = nullptr);

	~BuiltinClient() override;

	BuiltinClient(const BuiltinClient &) = delete;
	BuiltinClient(BuiltinClient &&) = delete;
	BuiltinClient &operator=(const BuiltinClient &) = delete;
	BuiltinClient &operator=(BuiltinClient &&) = delete;

signals:
	void builtinClientDestroyed(BuiltinClient *thisClient);
};

}

#endif
