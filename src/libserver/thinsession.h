// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_THINSESSION_H
#define DP_SERVER_THINSESSION_H
#include "libserver/session.h"
#include <QDeadlineTimer>

class QTimer;

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

	void readyToAutoReset(
		const AutoResetResponseParams &params, const QString &payload) override;

	StreamResetStartResult
	handleStreamResetStart(int ctxId, const QString &correlator) override;

	StreamResetAbortResult handleStreamResetAbort(int ctxId) override;

	StreamResetPrepareResult
	handleStreamResetFinish(int ctxId, int expectedMessageCount) override;

	void resolvePendingStreamedReset();

	void cleanupHistoryCache();

	bool supportsAutoReset() const override { return true; }

protected:
	void addToHistory(const net::Message &msg) override;
	void onSessionInitialized() override;
	void onSessionReset() override;
	void onClientJoin(Client *client, bool host) override;
	void onClientLeave(Client *client) override;
	void onClientDeop(Client *client) override;
	void onResetStream(Client &client, const net::Message &msg) override;
	void onStateChanged() override;

private:
	// Give up on autoreset requests after 3 minutes.
	static constexpr int AUTORESET_GIVE_UP_DELAY_MSECS = 180000;
	// Wait for up to 3 seconds after a client responded to an autorest query.
	static constexpr int AUTORESET_RESPONSE_DELAY_MSECS = 3000;
	// After an autoreset failed, wait 30 seconds before trying again.
	static constexpr int AUTORESET_FAILURE_RETRY_MSECS = 30000;

	enum class AutoResetState { NotSent, Queried, QueriedWaiting, Requested };

	struct AutoResetCandidate {
		int responseRank;
		int ctxId;
		ResetCapabilities capabilities;
		int osQuality;
		qreal netQuality;
		qreal averagePing;

		bool operator<(const AutoResetCandidate &other) const;
	};

	void sendStatusUpdate();
	void resetLastStatusUpdate() { m_lastStatusUpdate.setRemainingTime(10000); }

	void checkAutoResetQuery();
	static QString generateAutoResetPayload();
	void triggerAutoReset();
	void invalidateAutoResetCandidate(int ctxId);
	void clearAutoReset(int delay = 0);

	bool allClientsCaughtUpToResetStreamStart() const;

	QDeadlineTimer m_lastStatusUpdate;
	QDeadlineTimer m_lastSizeWarning;
	QDeadlineTimer m_autoResetDelay;
	AutoResetState m_autoResetRequestStatus = AutoResetState::NotSent;
	QTimer *m_autoResetTimer;
	QString m_autoResetPayload;
	QVector<AutoResetCandidate> m_autoResetCandidates;
};

}

#endif
