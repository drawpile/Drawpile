// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/proxy.h"
#include <QNetworkProxy>

namespace net {

static bool isBadProxy(const QNetworkProxy &proxy, bool allowHttp, bool listen)
{
	QNetworkProxy::ProxyType proxyType = proxy.type();
	switch(proxyType) {
	case QNetworkProxy::NoProxy:
		return false;
	case QNetworkProxy::Socks5Proxy:
		if(proxy.hostName().isEmpty()) {
			qWarning("Socks5 proxy with empty hostname, disabling");
			return true;
		} else if(!proxy.capabilities().testFlag(
					  QNetworkProxy::TunnelingCapability)) {
			qWarning(
				"Socks5 proxy doesn't have tunneling capability, disabling");
			return true;
		} else if(
			listen && !proxy.capabilities().testFlag(
						  QNetworkProxy::ListeningCapability)) {
			qWarning(
				"Socks5 proxy doesn't have listening capability, disabling");
			return true;
		} else {
			return false;
		}
	case QNetworkProxy::HttpProxy:
		if(!allowHttp) {
			qWarning("HTTP proxy on non-HTTP connection, disabling");
			return true;
		} else if(proxy.hostName().isEmpty()) {
			qWarning("HTTP proxy with empty hostname, disabling");
			return true;
		} else if(
			listen && !proxy.capabilities().testFlag(
						  QNetworkProxy::ListeningCapability)) {
			qWarning("HTTP proxy doesn't have listening capability, disabling");
			return true;
		} else {
			return false;
		}
	default:
		qWarning("Bad proxy type %d, unsetting it", int(proxyType));
		return true;
	}
}

bool shouldDisableProxy(
	int proxyMode, const QNetworkProxy &proxy, bool allowHttp, bool listen)
{
	if(proxyMode == int(ProxyMode::Disabled)) {
		return true;
	} else {
		if(proxyMode != int(ProxyMode::Default)) {
			qWarning("Unknown proxy mode %d, treating as default", proxyMode);
		}
		if(proxy.type() == QNetworkProxy::DefaultProxy) {
			return isBadProxy(
				QNetworkProxy::applicationProxy(), allowHttp, listen);
		} else {
			return isBadProxy(proxy, allowHttp, listen);
		}
	}
}

}
