// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/wasmpersistence.h"
#include "libclient/wasmsupport.h"
#include <QTimer>

static QTimer *syncPersistentFileSystemTimer;

extern "C" void drawpileInitScheduleSyncPersistentFileSystem()
{
	if(!syncPersistentFileSystemTimer) {
		syncPersistentFileSystemTimer = new QTimer;
		syncPersistentFileSystemTimer->setTimerType(Qt::CoarseTimer);
		syncPersistentFileSystemTimer->setSingleShot(true);
		syncPersistentFileSystemTimer->setInterval(500);
		QObject::connect(syncPersistentFileSystemTimer, &QTimer::timeout, []() {
			if(!browser::syncPersistentFileSystem()) {
				browser::scheduleSyncPersistentFileSystem();
			}
		});
	}
}

namespace browser {

void scheduleSyncPersistentFileSystem()
{
	if(syncPersistentFileSystemTimer) {
		syncPersistentFileSystemTimer->start();
	}
}

}
