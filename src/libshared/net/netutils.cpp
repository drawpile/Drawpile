// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/netutils.h"
#include <QRegularExpression>
#include <QUrlQuery>

namespace net {

QString addSchemeToUserSuppliedAddress(const QString &remoteAddress)
{
	bool haveValidScheme =
		remoteAddress.startsWith(
			QStringLiteral("drawpile://"), Qt::CaseInsensitive) ||
		remoteAddress.startsWith(
			QStringLiteral("ws://"), Qt::CaseInsensitive) ||
		remoteAddress.startsWith(QStringLiteral("wss://"), Qt::CaseInsensitive);
	if(haveValidScheme) {
		return remoteAddress;
	} else {
		QUrl url = QUrl::fromUserInput(remoteAddress);
		if(url.isValid()) {
			if(QString host = url.host(); !host.isEmpty()) {
				QUrl outUrl;
				outUrl.setScheme(QStringLiteral("drawpile"));
				outUrl.setHost(host);
				if(int port = url.port(); port > 0) {
					outUrl.setPort(port);
				}
				return outUrl.toString();
			}
		}
		return QStringLiteral("drawpile://") + remoteAddress;
	}
}

QUrl fixUpAddress(const QUrl &originalUrl, bool join)
{
#ifdef HAVE_WEBSOCKETS
	bool shouldTranslateToWebSocketUrl =
		originalUrl.scheme().compare(
			QStringLiteral("drawpile"), Qt::CaseInsensitive) == 0;
#	ifdef HAVE_TCPSOCKETS
	// If TCP is an option only translate if requested via query parameter.
	if(shouldTranslateToWebSocketUrl) {
		QUrlQuery originalQuery(originalUrl);
		if(!originalQuery.hasQueryItem(QStringLiteral("w")) &&
		   !originalQuery.hasQueryItem(QStringLiteral("W"))) {
			shouldTranslateToWebSocketUrl = false;
		}
	}
#	endif
	if(shouldTranslateToWebSocketUrl) {
		QUrl url = originalUrl;
		url.setScheme(
			looksLikeLocalhost(url.host()) ? QStringLiteral("ws")
										   : QStringLiteral("wss"));
		url.setPort(-1);

		QString path = url.path();
		url.setPath(QStringLiteral("/drawpile-web/ws"));

		QUrlQuery query(url);
		query.removeAllQueryItems(QStringLiteral("w"));
		query.removeAllQueryItems(QStringLiteral("W"));
		if(join) {
			QString autoJoinId = extractAutoJoinId(path);
			if(!autoJoinId.isEmpty()) {
				query.removeAllQueryItems(QStringLiteral("session"));
				query.addQueryItem(QStringLiteral("session"), autoJoinId);
			}
		}
		url.setQuery(query);

		qInfo(
			"Fixed up TCP URL '%s' to WebSocket URL '%s'",
			qUtf8Printable(originalUrl.toDisplayString()),
			qUtf8Printable(url.toDisplayString()));
		return url;
	}
#else
	if(originalUrl.scheme().compare(
		   QStringLiteral("ws"), Qt::CaseInsensitive) == 0 ||
	   originalUrl.scheme().compare(
		   QStringLiteral("wss"), Qt::CaseInsensitive) == 0) {
		QUrl url = originalUrl;
		url.setScheme(QStringLiteral("drawpile://"));
		url.setPort(-1);
		url.setPath(QString());
		qInfo(
			"Fixed up WebSocket URL '%s' to TCP URL '%s'",
			qUtf8Printable(originalUrl.toDisplayString()),
			qUtf8Printable(url.toDisplayString()));
		return url;
	}
#endif

	return originalUrl;
}

QString extractAutoJoinIdFromUrl(const QUrl &url)
{
	// Automatically join a session if the ID is included in the URL, either in
	// the path or in the "session" query parameter. WebSocket URLs only support
	// the latter, since they need the path to actually make a connection.
	QString path = extractSessionIdFromUrl(url);
	return extractAutoJoinId(path);
}

QString extractAutoJoinId(const QString &path)
{
	if(path.length() > 1) {
		static QRegularExpression idre("\\A/*([a-zA-Z0-9:-]{1,64})/*\\z");
		QRegularExpressionMatch m = idre.match(path);
		if(m.hasMatch()) {
			return m.captured(1);
		} else {
			qWarning(
				"Failed to extract auto-join id from '%s'",
				qUtf8Printable(path));
		}
	}
	return QString();
}

QString extractSessionIdFromUrl(const QUrl &url)
{
	QString path;
	if(!looksLikeWebSocketUrl(url)) {
		path = url.path();
	}

	if(path.isEmpty()) {
		path = QUrlQuery(url).queryItemValue(
			QStringLiteral("session"), QUrl::FullyDecoded);
	}

	static QRegularExpression re(QStringLiteral("\\A/+|/+\\z"));
	path.replace(re, QString());
	return path;
}

QUrl stripInviteCodeFromUrl(const QUrl &url, QString *outInviteCode)
{
	if(!url.scheme().startsWith(QStringLiteral("ws"), Qt::CaseInsensitive)) {
		QString path = url.path();
		if(!path.isEmpty()) {
			if(stripInviteCodeFromPath(path, outInviteCode)) {
				QUrl newUrl = url;
				newUrl.setPath(path);
				return newUrl;
			} else {
				return url;
			}
		}
	}

	QString key = QStringLiteral("session");
	QString path = QUrlQuery(url).queryItemValue(key, QUrl::FullyDecoded);
	if(!path.isEmpty() && stripInviteCodeFromPath(path, outInviteCode)) {
		QUrlQuery query(url);
		query.removeAllQueryItems(key);
		query.addQueryItem(key, path);
		QUrl newUrl = url;
		newUrl.setQuery(query);
		return newUrl;
	}

	if(outInviteCode) {
		outInviteCode->clear();
	}
	return url;
}

bool stripInviteCodeFromPath(QString &path, QString *outInviteCode)
{
	QString sessionId = extractAutoJoinId(path);
	if(sessionId.isEmpty()) {
		return false;
	}

	int index = sessionId.lastIndexOf(':');
	if(index == -1) {
		return false;
	}

	QString inviteCode = sessionId.mid(index + 1);
	if(inviteCode.isEmpty()) {
		return false;
	}

	path.replace(
		QRegularExpression(QStringLiteral(":%1(/?)\\z")
							   .arg(QRegularExpression::escape(inviteCode))),
		QStringLiteral("\\1"));

	if(outInviteCode) {
		*outInviteCode = inviteCode;
	}
	return true;
}

void setSessionIdOnUrl(QUrl &url, QString &sessionId)
{
	QUrlQuery query(url);
	QString key = QStringLiteral("session");
	query.removeAllQueryItems(key);
	if(looksLikeWebSocketUrl(url)) {
		query.addQueryItem(key, sessionId);
	} else {
		url.setPath(QStringLiteral("/") + sessionId);
	}
	url.setQuery(query);
}

bool looksLikeWebSocketUrl(const QUrl &url)
{
	return url.scheme().startsWith(QStringLiteral("ws"), Qt::CaseInsensitive);
}

bool looksLikeLocalhost(const QString &host)
{
	return host.startsWith(QStringLiteral("localhost"), Qt::CaseInsensitive) ||
		   host.startsWith(QStringLiteral("127.0.0.1")) ||
		   host.startsWith(QStringLiteral("::1"));
}

}
