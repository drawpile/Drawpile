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

#include "thinsrv/gui/singleinstance.h"

namespace server {
namespace gui {

static const QString SEMAPHORE_KEY = "drawpile-srv-gui-S";
static const QString MEM_KEY = "drawpile-srv-gui-M";

SingleInstance::SingleInstance(QObject *parent)
	: QObject(parent),
	  m_sharedmem(MEM_KEY, this),
	  m_semaphore(SEMAPHORE_KEY, 1)
{
}

SingleInstance::~SingleInstance()
{
	m_semaphore.acquire();
	if(m_sharedmem.isAttached()) {
		qDebug("Detaching SingleInstance shared memory segment.");
		m_sharedmem.detach();
	}
	m_semaphore.release();
}

bool SingleInstance::tryStart()
{
	// Try to create a shared memory segment. If it exists already,
	// we know another instance is running.
	// This would not be needed if QSystemSemaphore had a tryAcquire() function.

	m_semaphore.acquire();
	{
		// On UNIX, the shared memory segment is not automatically deleted when the
		// last process ends. If the server crashes, the segment will stick around.
		// Attaching and detaching will clean up the stale segment if it exists.
		QSharedMemory cleanup(MEM_KEY);
		cleanup.attach();
	}

	const bool created = m_sharedmem.create(1);
	m_semaphore.release();

	qDebug("SingleInstance shared memory segment %s.", created ? "created" : "already exists");

	return created;
}

}
}
