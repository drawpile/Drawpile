/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef DP_SERVER_SESSION_HISTORY_H
#define DP_SERVER_SESSION_HISTORY_H

#include "../shared/net/message.h"

#include <QObject>
#include <tuple>

namespace server {

/**
 * @brief Abstract base class for session history implementations
 *
 */
class SessionHistory : public QObject {
	Q_OBJECT
public:
	SessionHistory(QObject *parent);

	/**
	 * @brief Add a new message to the history
	 *
	 * The signal newMessagesAvailable() will be emitted.
	 *
	 * @return false if adding this message would put the history over the size limit
	 */
	bool addMessage(const protocol::MessagePtr &msg);

	/**
	 * @brief Reset the session history
	 *
	 * Resetting replaces the session history and size with the new
	 * set, but index number continue as they were.
	 *
	 * The signal newMessagesAvailable() will be emitted.
	 *
	 * @return false if new history is larger than the size limit
	 */
	bool reset(const QList<protocol::MessagePtr> &newHistory);

	/**
	 * @brief Get a batch of messages
	 *
	 * This returns a {batch, lastIndex} tuple.
	 * The batch contains zero or more messages immediately following
	 * the given index.
	 *
	 * The second element of the tuple is the index of the last message
	 * in the batch, or lastIndex() if there were no more available messages
	 */
	virtual std::tuple<QList<protocol::MessagePtr>, int> getBatch(int after) const = 0;

	/**
	 * @brief Mark messages before the given index as unneeded (for now)
	 *
	 * This is used to inform caching history storage backends which messages
	 * have been sent to all connected users and can thus be freed.
	 */
	virtual void cleanupBatches(int before) = 0;

	/**
	 * @brief Set the size limit for the history.
	 *
	 * The size limit is checked when new messages are added to the session.
	 *
	 * @param limit maximum size in bytes or 0 for no limit
	 */
	void setSizeLimit(uint limit) { m_sizeLimit = limit; }

	/**
	 * @brief Get the session size limit
	 *
	 * If zero, the size is not limited.
	 */
	uint sizeLimit() const { return m_sizeLimit; }

	/**
	 * @brief Get the size of the history in bytes
	 *
	 * Note: this is the serialized size, not the size of the in-memory
	 * representation.
	 */
	uint sizeInBytes() const { return m_sizeInBytes; }

	/**
	 * @brief Get the index number of the first message in history
	 *
	 * The index numbers grow monotonically during the session and are
	 * not reset even when the history itself is reset.
	 */
	int firstIndex() const { return m_firstIndex; }

	/**
	 * @brief Get the index number of the last message in history
	 */
	int lastIndex() const { return m_lastIndex; }

signals:
	/**
	 * @brief This signal is emited when new messages are added to the history
	 *
	 * Clients whose upload queues are empty should get the new message batch and
	 * start sending.
	 */
	void newMessagesAvailable();

protected:
	virtual void historyAdd(const protocol::MessagePtr &msg) = 0;
	virtual void historyReset(const QList<protocol::MessagePtr> &newHistory) = 0;

private:
	uint m_sizeInBytes;
	uint m_sizeLimit;
	int m_firstIndex;
	int m_lastIndex;
};

}

#endif

