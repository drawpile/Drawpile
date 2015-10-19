/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef COMMANDQUEUE_H
#define COMMANDQUEUE_H

#include <QList>
#include <QObject>
#include <QEvent>

#include "../shared/net/message.h"

namespace canvas {

class CanvasModel;

class CommandEvent : public QEvent
{
public:
	CommandEvent(protocol::MessagePtr msg, bool local)
	: QEvent(commandEventType()), m_msg(msg), m_local(local)
	{}
 
	static Type commandEventType() {
		static const Type eventType = Type(registerEventType());
		return eventType;
	}

	protocol::MessagePtr message() const { return m_msg; }
	bool isLocal() const { return m_local; }

private:
	protocol::MessagePtr m_msg;
	bool m_local;
};

/**
 * @brief A companion class to CanvasModel that handles commands in a separate thread.
 */
class CommandQueue : public QObject
{
	Q_OBJECT
public:
	explicit CommandQueue(CanvasModel *canvas, QObject *parent=nullptr);

	bool event(QEvent *e);

private:
	void handleCommand(protocol::MessagePtr msg);
	void handleLocalCommand(protocol::MessagePtr msg);

	CanvasModel *m_canvas;
};

}

#endif // COMMANDQUEUE_H
