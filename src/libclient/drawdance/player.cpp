// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/input.h>
}
#include "libclient/drawdance/player.h"
#include <QString>

namespace drawdance {

Player::~Player()
{
	DP_player_free(m_data);
}

bool Player::open(
	DP_PlayerType type, const QString &path, DP_LoadResult *outResult)
{
	DP_Player *player =
		DP_player_new_from_path(type, path.toUtf8().constData(), outResult);
	if(player) {
		DP_player_free(m_data);
		m_data = player;
		return true;
	} else {
		return false;
	}
}

void Player::close()
{
	DP_player_free(m_data);
	m_data = nullptr;
}

DP_PlayerResult Player::step(bool decodeOpaque, net::Message &outMessage)
{
	DP_Message *msg = nullptr;
	DP_PlayerResult result = DP_player_step(m_data, decodeOpaque, &msg);
	outMessage = net::Message::noinc(msg);
	return result;
}

}
