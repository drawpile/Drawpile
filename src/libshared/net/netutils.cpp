// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/net/netutils.h"
#include <QHostAddress>
#include <QRegularExpression>
#include <QUrlQuery>

static_assert(
	int(net::ConnectStrategy::Preference) == 0,
	"Preference connect strategy is 0, since that's used as a default");

namespace net {

int defaultConnectStrategy()
{
	return int(ConnectStrategy::Guess);
}

int resolveConnectStrategy(int input, int preference)
{
	switch(input) {
	case int(ConnectStrategy::Preference):
		break;
	case int(ConnectStrategy::Literal):
	case int(ConnectStrategy::ForceWebSocket):
	case int(ConnectStrategy::Guess):
#ifdef HAVE_TCPSOCKETS
	case int(ConnectStrategy::ForceTcp):
#endif
		return input;
	default:
		qWarning("resolveConnectStrategy: unknown input %d", input);
		break;
	}
	return resolveConnectStrategy(preference, defaultConnectStrategy());
}

QString connectStrategyToString(int input)
{
	switch(input) {
	case int(ConnectStrategy::Preference):
		return QStringLiteral("preference");
	case int(ConnectStrategy::Literal):
		return QStringLiteral("literal");
	case int(ConnectStrategy::ForceWebSocket):
		return QStringLiteral("ws");
	case int(ConnectStrategy::ForceTcp):
		return QStringLiteral("tcp");
	case int(ConnectStrategy::Guess):
		return QStringLiteral("guess");
	}
	qWarning("connectStrategyToString: unknown input %d", int(input));
	return QString();
}

int connectStrategyFromString(const QString &s)
{
	if(s.isEmpty() ||
	   QStringLiteral("preference").compare(s, Qt::CaseInsensitive) == 0) {
		return int(ConnectStrategy::Preference);
	} else if(QStringLiteral("literal").compare(s, Qt::CaseInsensitive) == 0) {
		return int(ConnectStrategy::Literal);
	} else if(QStringLiteral("ws").compare(s, Qt::CaseInsensitive) == 0) {
		return int(ConnectStrategy::ForceWebSocket);
	} else if(QStringLiteral("tcp").compare(s, Qt::CaseInsensitive) == 0) {
		return int(ConnectStrategy::ForceTcp);
	} else if(QStringLiteral("guess").compare(s, Qt::CaseInsensitive) == 0) {
		return int(ConnectStrategy::Guess);
	} else {
		qWarning(
			"connectStrategyFromString: unknown input '%s'", qUtf8Printable(s));
		return int(ConnectStrategy::Preference);
	}
}

QStringList connectStrategyStrings()
{
	int count = int(ConnectStrategy::Last) + 1;
	QStringList strings;
	strings.reserve(count);
	for(int i = 0; i < count; ++i) {
		strings.append(connectStrategyToString(i));
	}
	return strings;
}

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
	bool shouldTranslateToWebSocketUrl =
		originalUrl.scheme().compare(
			QStringLiteral("drawpile"), Qt::CaseInsensitive) == 0;
#ifdef HAVE_TCPSOCKETS
	// If TCP is an option only translate if requested via query parameter.
	if(shouldTranslateToWebSocketUrl) {
		QUrlQuery originalQuery(originalUrl);
		if(!originalQuery.hasQueryItem(QStringLiteral("w")) &&
		   !originalQuery.hasQueryItem(QStringLiteral("W"))) {
			shouldTranslateToWebSocketUrl = false;
		}
	}
#endif
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

	return originalUrl;
}

QUrl convertToTcpUrl(const QUrl &originalUrl, bool join)
{
	if(!looksLikeWebSocketUrl(originalUrl)) {
		return originalUrl;
	}

	QUrl url = originalUrl;
	url.setScheme(QStringLiteral("drawpile"));
	url.setPath(QString());

	QUrlQuery query(url);
	query.removeAllQueryItems(QStringLiteral("w"));
	query.removeAllQueryItems(QStringLiteral("W"));
	if(join) {
		QString autoJoinId =
			query.queryItemValue(QStringLiteral("session"), QUrl::FullyDecoded);
		if(!autoJoinId.isEmpty()) {
			if(!autoJoinId.startsWith("/")) {
				autoJoinId.prepend("/");
			}
			url.setPath(autoJoinId);
		}
	}
	query.removeAllQueryItems(QStringLiteral("session"));
	url.setQuery(query);

	return url;
}

QUrl convertToWebSocketUrl(const QUrl &originalUrl, bool join)
{
	if(looksLikeWebSocketUrl(originalUrl)) {
		return originalUrl;
	}

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

	return url;
}

namespace {
static bool isTcpUrlWithWebSocketParameter(const QUrl &url)
{
	if(!looksLikeWebSocketUrl(url)) {
		QUrlQuery query(url);
		if(query.hasQueryItem(QStringLiteral("w")) ||
		   query.hasQueryItem(QStringLiteral("W"))) {
			return true;
		}
	}
	return false;
}
}

QUrl convertUrl(
	const QUrl &originalUrl, bool join, int connectStrategy,
	bool isFirstAttempt, bool *outTentative)
{
	QUrl url;
	bool tentative = false;

#ifdef HAVE_TCPSOCKETS
	switch(connectStrategy) {
	case int(ConnectStrategy::ForceWebSocket):
		url = convertToWebSocketUrl(originalUrl, join);
		break;
	case int(ConnectStrategy::ForceTcp):
		url = convertToTcpUrl(originalUrl, join);
		break;
	case int(ConnectStrategy::Literal):
		if(isTcpUrlWithWebSocketParameter(originalUrl)) {
			url = convertToWebSocketUrl(originalUrl, join);
		} else {
			url = originalUrl;
		}
		break;
	default:
		if(isFirstAttempt && guessWebSocketSupport(originalUrl)) {
			url = convertToWebSocketUrl(originalUrl, join);
			tentative = true;
		} else {
			url = convertToTcpUrl(originalUrl, join);
		}
		break;
	}
#else
	Q_UNUSED(connectStrategy);
	Q_UNUSED(isFirstAttempt);
	url = convertToWebSocketUrl(originalUrl, join);
#endif

	if(outTentative) {
		*outTentative = tentative;
	}
	return url;
}

QString censorUrlForLogging(const QUrl &originalUrl)
{
	QUrl url = originalUrl;
	QString censor = QStringLiteral("***");

	if(!url.fragment().isEmpty()) {
		url.setFragment(censor);
	}

	if(!url.userName().isEmpty()) {
		url.setUserName(censor);
	}

	if(!url.password().isEmpty()) {
		url.setPassword(censor);
	}

	QUrlQuery query(url);
	QList<QPair<QString, QString>> queryItems = query.queryItems();
	for(int i = 0, count = queryItems.size(); i < count; ++i) {
		if(QStringLiteral("p").compare(
			   queryItems[i].first, Qt::CaseInsensitive) == 0) {
			queryItems[i].second = censor;
		}
	}
	query.setQueryItems(queryItems);
	url.setQuery(query);

	QString displayUrl = url.toDisplayString();

	QString inviteCode;
	stripInviteCodeFromUrl(url, &inviteCode);
	if(!inviteCode.isEmpty()) {
		inviteCode.prepend(':');
		censor.prepend(':');
		displayUrl.replace(inviteCode, censor);
	}

	return displayUrl;
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

bool guessWebSocketSupport(const QUrl &url)
{
	// Explicit WebSocket URL definitely supports WebSockets.
	if(looksLikeWebSocketUrl(url)) {
		return true;
	}

	// If this is a TCP URL with a ?w on it, we should also try it.
	if(isTcpUrlWithWebSocketParameter(url)) {
		return true;
	}

	// Localhost is a bad candidate for WebSockets.
	QString host = url.host(QUrl::FullyDecoded);
	if(looksLikeLocalhost(host)) {
		return false;
	}

	// A raw IP address probably won't work either. Also yes, weird interface:
	// setAddress returns whether parsing the the IP address succeeded.
	if(QHostAddress().setAddress(host)) {
		return false;
	}

	// Anything else should be a regular host, which should generally have
	// WebSockets set up properly. Worth giving it a shot.
	return true;
}

}
