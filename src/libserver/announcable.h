// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANNOUNCABLE_H
#define ANNOUNCABLE_H

class QString;

namespace sessionlisting {

struct Session;

/**
 * Interface class for announcable sessions
 */
class Announcable {
public:
	virtual ~Announcable();

	//! Get the ID of the announcable session
	virtual QString id() const = 0;

	//! Get an announcement for this session
	virtual Session getSessionAnnouncement() const = 0;

	//! Send a message to the users of this session
	virtual void sendListserverMessage(const QString &message) = 0;
};

}

#endif // ANNOUNCABLE_H
