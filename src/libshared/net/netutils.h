// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_NETUTILS_H
#define LIBSHARED_NET_NETUTILS_H
#include <QString>
#include <QStringList>
#include <QUrl>

namespace net {

// Decides how connection attempts should be made.
enum class ConnectStrategy {
	// Overridable value for potentially making this a user preference. To do
	// so, this would need to get wired up in mainwindow.cpp where it currently
	// calls resolveConnectStrategy with defaultConnectStrategy. Keep this value
	// at 0, some code uses that as the default value!
	Preference,
	// Use the address as written.
	Literal,
	// Connect via WebSocket, converting TCP addresses.
	ForceWebSocket,
	// Connect via TCP, converting WebSocket addresses.
	ForceTcp,
	Last = ForceWebSocket,
};

int defaultConnectStrategy();

int resolveConnectStrategy(int input, int preference);

QString connectStrategyToString(int input);
int connectStrategyFromString(const QString &s);
QStringList connectStrategyStrings();

QString addSchemeToUserSuppliedAddress(const QString &remoteAddress);

QUrl convertToTcpUrl(const QUrl &originalUrl, bool join);
QUrl convertToWebSocketUrl(const QUrl &originalUrl, bool join);
QUrl convertUrl(const QUrl &originalUrl, bool join, int connectStrategy);

QString censorUrlForLogging(const QUrl &originalUrl);

QString extractAutoJoinIdFromUrl(const QUrl &url);

QString extractAutoJoinId(const QString &path);

QString extractSessionIdFromUrl(const QUrl &url);

QUrl stripInviteCodeFromUrl(const QUrl &url, QString *outInviteCode = nullptr);

bool stripInviteCodeFromPath(QString &path, QString *outInviteCode = nullptr);

void setSessionIdOnUrl(QUrl &url, QString &sessionId);

bool looksLikeWebSocketUrl(const QUrl &url);

bool looksLikeLocalhost(const QString &host);

}

#endif
