// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_PROXY_H
#define LIBSHARED_NET_PROXY_H

class QNetworkProxy;

namespace net {

enum class ProxyMode {
	Default = 0,
	Disabled = 1,
};

bool shouldDisableProxy(
	int proxyMode, const QNetworkProxy &proxy, bool allowHttp, bool listen);

}

#endif
