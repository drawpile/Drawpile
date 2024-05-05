// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_WASMSUPPORT_H
#define LIBCLIENT_WASMSUPPORT_H
#include <QString>

class QByteArray;
class QUrl;

namespace net {
class LoginHandler;
}

namespace browser {

bool hasLowPressurePen();
QString getWelcomeMessage();
void showLoginModal(net::LoginHandler *loginHandler);
void cancelLoginModal(net::LoginHandler *loginHandler);
void authenticate(net::LoginHandler *loginHandler, const QByteArray &payload);
void intuitFailedConnectionReason(QString &description, const QUrl &url);
// Tries to mount IDBFS file system under /appdata, calls drawpileMain after.
void mountPersistentFileSystem(int argc, char **argv);
// Asynchronously tries to persist the IDBFS file system, returns false if
// another sync operation is already in progress.
bool syncPersistentFileSystem();

}

#endif
