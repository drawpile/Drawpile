// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_NETUTILS_H
#define LIBSHARED_NET_NETUTILS_H
#include <QString>
#include <QUrl>

namespace net {

QString addSchemeToUserSuppliedAddress(const QString &remoteAddress);

QUrl fixUpAddress(const QUrl &originalUrl, bool join);

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
