// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_SERVER_BUILTINSESSION_H
#define LIBCLIENT_SERVER_BUILTINSESSION_H
#include "libclient/drawdance/aclstate.h"
#include "libclient/server/builtinreset.h"
#include "libserver/session.h"
#include <QSharedPointer>

namespace canvas {
class PaintEngine;
}

namespace drawdance {
class CanvasState;
}

namespace server {

class BuiltinSession final : public Session {
	Q_OBJECT
public:
	BuiltinSession(
		ServerConfig *config, sessionlisting::Announcements *announcements,
		canvas::PaintEngine *paintEngine, const QString &id,
		const QString &idAlias, const QString &founder,
		QObject *parent = nullptr);

	BuiltinSession(const BuiltinSession &) = delete;
	BuiltinSession(BuiltinSession &&) = delete;
	BuiltinSession &operator=(const BuiltinSession &) = delete;
	BuiltinSession &operator=(BuiltinSession &&) = delete;

	bool supportsAutoReset() const override;
	void readyToAutoReset(
		const AutoResetResponseParams &params, const QString &payload) override;

	void doInternalReset(const drawdance::CanvasState &canvasState);

	StreamResetStartResult
	handleStreamResetStart(int ctxId, const QString &correlator) override;

	StreamResetAbortResult handleStreamResetAbort(int ctxId) override;

	StreamResetPrepareResult
	handleStreamResetFinish(int ctxId, int expectedMessageCount) override;

protected:
	void addToHistory(const net::Message &msg) override;
	void onSessionInitialized() override;
	void onSessionReset() override;
	void onClientJoin(Client *client, bool host) override;
	void onClientDeop(Client *client) override;
	void onResetStream(Client &client, const net::Message &msg) override;
	void onStateChanged() override;
	void chatMessageToAll(const net::Message &msg) override;

private:
	bool haveAnyClientAwaitingReset() const;
	void startInternalReset(const drawdance::CanvasState &canvasState);
	void finishInternalReset();

	canvas::PaintEngine *m_paintEngine;
	drawdance::AclState m_acls;
	QString m_pinnedMessage;
	int m_defaultLayer = 0;
	bool m_softResetRequested = false;
	QSharedPointer<BuiltinReset> m_reset;
};
}

#endif
