// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_THINSESSION_H
#define DP_SERVER_THINSESSION_H
#include "libserver/session.h"
#include <QDeadlineTimer>

namespace server {

/**
 * The (thin) serverside session state.
 */
class ThinSession final : public Session {
	Q_OBJECT
public:
	ThinSession(
		SessionHistory *history, ServerConfig *config,
		sessionlisting::Announcements *announcements,
		QObject *parent = nullptr);

	void readyToAutoReset(int ctxId) override;

	void cleanupHistoryCache();

	bool supportsAutoReset() const override { return true; }

protected:
	void addToHistory(const net::Message &msg) override;
	void onSessionReset() override;
	void onClientJoin(Client *client, bool host) override;

private:
	enum class AutoResetState { NotSent, Queried, Requested };

	void resetLastStatusUpdate()
	{
		m_lastStatusUpdate.setRemainingTime(10000);
	}

	QDeadlineTimer m_lastStatusUpdate;
	QDeadlineTimer m_lastSizeWarning;

	AutoResetState m_autoResetRequestStatus = AutoResetState::NotSent;
};

}

#endif
