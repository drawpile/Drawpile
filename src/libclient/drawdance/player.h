// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_PLAYER_H
#define LIBCLIENT_DRAWDANCE_PLAYER_H
extern "C" {
#include <dpengine/player.h>
}
#include "libclient/net/message.h"

class QString;

namespace drawdance {

class Player final {
public:
	Player() = default;
	~Player();

	Player(const Player &) = delete;
	Player(Player &&) = delete;
	Player &operator=(const Player &) = delete;
	Player &operator=(Player &&) = delete;

	DP_Player *get() { return m_data; }

	bool open(
		DP_PlayerType type, const QString &path,
		DP_LoadResult *outResult = nullptr);

	void close();

	bool isCompatible() const { return DP_player_compatible(m_data); }

	double progress() const { return DP_player_progress(m_data); }

	DP_PlayerResult step(bool decodeOpaque, net::Message &outMessage);

private:
	DP_Player *m_data = nullptr;
};

}

#endif
