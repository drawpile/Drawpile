// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/canvas_history.h>
#include <dpmsg/acl.h>
}
#include "libclient/canvas/reconnectstate.h"
#include "libclient/drawdance/aclstate.h"

namespace canvas {

ReconnectState::ReconnectState(
	const QJsonObject &sessionConfig, const HistoryIndex &hi,
	const QVector<User> &users, const drawdance::AclState &aclState,
	QObject *parent)
	: QObject(parent)
	, m_sessionConfig(sessionConfig)
	, m_historyIndex(hi)
	, m_users(users)
	, m_aclState(DP_acl_state_new_clone(aclState.get(), aclState.localUserId()))
{
}

ReconnectState::~ReconnectState()
{
	DP_canvas_history_reconnect_state_free(m_chrs);
	DP_acl_state_free(m_aclState);
}

void ReconnectState::clearDetach()
{
	m_historyIndex.clear();
	m_users.clear();
	m_aclState = nullptr;
	m_chrs = nullptr;
}

}
