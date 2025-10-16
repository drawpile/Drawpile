// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_RECONNECTSTATE_H
#define LIBCLIENT_CANVAS_RECONNECTSTATE_H
#include "libclient/canvas/userlist.h"
#include "libshared/util/historyindex.h"
#include "libshared/util/qtcompat.h"
#include <QJsonObject>
#include <QObject>

struct DP_AclState;
struct DP_CanvasHistoryReconnectState;

namespace drawdance {
class AclState;
}

namespace canvas {

class ReconnectState : public QObject {
	Q_OBJECT
	COMPAT_DISABLE_COPY_MOVE(ReconnectState)
public:
	ReconnectState(
		const QJsonObject &sessionConfig, const HistoryIndex &hi,
		const QVector<User> &users, const drawdance::AclState &aclState,
		int defaultLayerId, QObject *parent = nullptr);

	~ReconnectState() override;

	const QJsonObject &sessionConfig() const { return m_sessionConfig; }
	QJsonObject &&takeSessionConfig() { return std::move(m_sessionConfig); }

	const HistoryIndex &historyIndex() const { return m_historyIndex; }

	const QVector<User> users() const { return m_users; }

	DP_AclState *aclState() const { return m_aclState; }

	DP_CanvasHistoryReconnectState *historyState() const { return m_chrs; }

	void setHistoryState(DP_CanvasHistoryReconnectState *chrs)
	{
		m_chrs = chrs;
	}

	int defaultLayerId() const { return m_defaultLayerId; }

	void clearDetach();

private:
	QJsonObject m_sessionConfig;
	HistoryIndex m_historyIndex;
	QVector<User> m_users;
	DP_AclState *m_aclState;
	DP_CanvasHistoryReconnectState *m_chrs = nullptr;
	int m_defaultLayerId = 0;
};

}

#endif
