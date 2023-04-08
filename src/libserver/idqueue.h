// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_SRV_IDQUEUE_H
#define DP_SRV_IDQUEUE_H

#include <QList>
#include <QHash>
#include <cstdint>

namespace server {

class IdQueue {
public:
	static const uint8_t FIRST_ID = 1;
	static const uint8_t LAST_ID = 254;
	IdQueue();

	/**
	 * @brief Set the last ID assigned for the given username
	 *
	 * This is used to reserve specific IDs for given usernames.
	 * The reservation is not hard: other users may get assigned the same
	 * ID if it reaches the front of the queue.
	 *
	 * Calling this will move the ID to the end of the queue
	 */
	void setIdForName(uint8_t id, const QString &name);

	/**
	 * @brief Get the ID last used by this username
	 *
	 * If the name has not been seen before, 0 is returned.
	 */
	uint8_t getIdForName(const QString &name) const;

	/**
	 * @brief Take the ID at the front of the queue
	 *
	 * The ID at the front of the queue is returned and moved
	 * to the back.
	 */
	uint8_t nextId();

	/**
	 * @brief Move the given ID to the back of the queue
	 */
	void reserveId(uint8_t id);

private:
	QList<uint8_t> m_ids;
	QHash<QString,uint8_t> m_nameIds;
};

}

#endif

