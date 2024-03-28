// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_WASMSUPPORT_H
#define LIBCLIENT_WASMSUPPORT_H

class QByteArray;

namespace net {
class LoginHandler;
}

namespace browser {

bool hasLowPressurePen();
void showLoginModal(net::LoginHandler *loginHandler);
void cancelLoginModal(net::LoginHandler *loginHandler);
void authenticate(net::LoginHandler *loginHandler, const QByteArray &payload);

}

#endif
