/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef QMICROHTTPD_H
#define QMICROHTTPD_H

#include "libshared/util/qtcompat.h"

#include <QObject>
#include <QRegularExpression>
#include <QHostAddress>
#include <QByteArray>
#include <QHash>

#include <functional>

class QJsonDocument;
class HttpResponse;
class HttpRequest;
typedef std::function<HttpResponse(const HttpRequest &)> HttpRequestHandler;
typedef std::function<bool(const QHostAddress &)> AcceptPolicy;

/**
 * A Qt style wrapper for GNU libmicrohttpd
 */
class MicroHttpd final : public QObject {
Q_OBJECT
public:
	struct Private;
	explicit MicroHttpd(QObject *parent = nullptr);
	~MicroHttpd() override;

	/**
	 * Start listening on the given port.
	 *
	 * @param port HTTP server port
	 */
	bool listen(quint16 port);

	/**
	 * Start listening on the given socket
	 * @param socket file descriptor
	 */
	bool listenFd(int socket);
	
	void stop();

	/**
	 * @brief Set connection accept policy.
	 *
	 * By default, only connections from localhost are allowed.
	 * This should be called before listen()
	 *
	 * @param ap
	 */
	void setAcceptPolicy(const AcceptPolicy &ap);

	/**
	 * @brief Add a new request handler.
	 *
	 * Note! The handler will run in the HTTP server thread!
	 */
	void addRequestHandler(const QRegularExpression &urlMatcher, HttpRequestHandler handler);
	void addRequestHandler(const char *urlMatcher, HttpRequestHandler handler);

	//! Set server-wide basic authentication
	void setBasicAuth(const QString &realm, const QString &username, const QString &password);

	//! Get libmicrohttpd version
	static QString version();

private:
	Private *_d;
};

class HttpRequest {
public:
	enum Method {HEAD, GET, POST, PUT, DELETE};

	HttpRequest() {}
	HttpRequest(Method method, const QString &path)
		: _method(method), _path(path)
	{}

	//! The HTTP method
	Method method() const { return _method; }

	//! The path of the request
	const QString &path() const { return _path; }

	//! The regular expression match object that triggered this handler
	const QRegularExpressionMatch pathMatch() const { return _match; }

	//! HTTP headers (note: all lower case)
	const QHash<QString, QString> &headers() const { return _headers; }
	
	//! POST arguments
	const QHash<QString, QString> &postData() const { return _postdata; }

	//! GET arguments
	const QHash<QString, QString> &getData() const { return _getdata; }

	//! Request body
	const QByteArray body() const { return _body; }

	void setUrlMatch(const QRegularExpressionMatch &match) { _match = match; }
	void setHeaders(const QHash<QString,QString> &data);
	void setPostData(const QHash<QString,QString> &data) { _postdata = data; }
	void setGetData(const QHash<QString,QString> &data) { _getdata = data; }
	void addBodyData(const char *data, size_t len) {
		_body.append(data, compat::castSize(len));
	}

private:
	Method _method;
	QString _path;
	QRegularExpressionMatch _match;
	QHash<QString, QString> _headers;
	QHash<QString, QString> _postdata;
	QHash<QString, QString> _getdata;
	QByteArray _body;
};

class HttpResponse {
public:
	explicit HttpResponse(int code, const QByteArray &body=QByteArray()) : _code(code), _body(body) { }

	//! Return a response configured for HTML content
	static HttpResponse HtmlResponse(const QString &html);

	//! Return a file from the filesystem
	static HttpResponse FileResponse(const QString &path, bool head=false);

	//! Return a JSON document
	static HttpResponse JsonResponse(const QJsonDocument &doc, int statuscode=200);

	//! Return an error message
	static HttpResponse JsonErrorResponse(const QString &message, int statuscode);

	//! Return a Method Not Allowed error
	static HttpResponse MethodNotAllowed(const QStringList &allow);

	//! Return a Not Found error
	static HttpResponse NotFound();

	int code() const { return _code; }
	void setBody(const QByteArray &body) { _body = body; }
	const QByteArray body() const { return _body; }

	void setHeader(const QString &key, const QString &value) { _headers[key] = value; }
	const QHash<QString,QString> &headers() const { return _headers; }

private:
	int _code;
	QByteArray _body;
	QHash<QString, QString> _headers;
};

#endif

