// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/singleinstance.h"

namespace server {
namespace gui {

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
