// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_WASMPERSISTENCE_H
#define LIBCLIENT_UTILS_WASMPERSISTENCE_H

// In the browser, files that go into paths::writableDataLocation are stored
// under /assets. This is a directory mounted using IDBFS, which can synchronize
// the data into IndexedDB. This sync operation is explicit though, which this
// file provides macros to do. DRAWPILE_FS_PERSIST immediately schedules a sync
// operation, while DRAWPILE_FS_PERSIST_SCOPE declares an object that schedules
// the sync upon its destruction. These sync operations are on a debounce timer,
// so it's okay to call them repeatedly. Outside of Emscripten, they are noops.

#ifdef __EMSCRIPTEN__
#	define DRAWPILE_FS_PERSIST() browser::scheduleSyncPersistentFileSystem()
#	define DRAWPILE_FS_PERSIST_SCOPE(X)                                       \
		browser::ScopedPersistentFileSystemSync X

namespace browser {

void scheduleSyncPersistentFileSystem();

struct ScopedPersistentFileSystemSync {
	~ScopedPersistentFileSystemSync() { scheduleSyncPersistentFileSystem(); }
};

}

#else
#	define DRAWPILE_FS_PERSIST() /* nothing */
#	define DRAWPILE_FS_PERSIST_SCOPE(X)                                       \
		do {                                                                   \
			/* nothing */                                                      \
		} while(0)
#endif

#endif
