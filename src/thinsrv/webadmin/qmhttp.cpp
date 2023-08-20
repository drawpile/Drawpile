// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/webadmin/qmhttp.h"
#include "libshared/util/qtcompat.h"

#include <QList>
#include <QPair>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDateTime>
#include <QMimeDatabase>
#include <QFile>

#include <microhttpd.h>
#undef DELETE // winnt.h

/*
 * In version 0.9.71 of libmicrohttpd, MHD_YES and MHD_NO have been changed
 * from being #defines to being enum values. Several callback types have also
 * changed from returning an int to returning an MHD_Result.
 *
 * That's fine in C, but it actually breaks in C++ and errors out, so we have
 * to add this compatibility shim to make things work again.
 *
 * https://lists.gnu.org/archive/html/libmicrohttpd/2020-06/msg00013.html
 */
#if MHD_VERSION < 0x00097002
typedef int MHD_Result;
#endif

static const char *MSG_401 = "<html><body>401 - Unauthorized</body></html>";
static const char *MSG_404 = "<html><body>404 - Page not found</body></html>";

struct MicroHttpd::Private {
	MHD_Daemon *mhd = nullptr;
	QList<QPair<QRegularExpression, HttpRequestHandler>> requesthandlers;

	// Basic auth
	QByteArray baRealm;
	QByteArray baUser;
	QByteArray baPass;

	QByteArray allowedOrigin;

	// Access controls
	AcceptPolicy acceptPolicy;

	Private()
	{
		acceptPolicy = [](const QHostAddress &addr) { return addr.isLoopback(); };
	}
};

namespace {

struct RequestContext {
	HttpRequest request;
	HttpRequestHandler reqhandler;

	MHD_PostProcessor *postprocessor;

	QHash<QString, QByteArray> postdata;

	RequestContext(const HttpRequest &req, const HttpRequestHandler &rh)
		: request(req), reqhandler(rh), postprocessor(nullptr)
	{ }
	~RequestContext()
	{
		if(postprocessor)
			MHD_destroy_post_processor(postprocessor);
	}
};

static void logMessage(MHD_Connection *connection, int statusCode, const char *method, const char *url)
{
	QHostAddress clientAddress(MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr);

	qInfo("%s HTTP(%d) %s \"%s %s\"",
			qPrintable(QDateTime::currentDateTime().toString(Qt::ISODate)),
			statusCode, qPrintable(clientAddress.toString()), method, url);
}

MHD_Result iterate_post(void *con_cls, enum MHD_ValueKind kind, const char *key, const char *filename, const char *content_type, const char *transfer_encoding, const char *data, uint64_t off, size_t size)
{
	Q_UNUSED(kind);
	Q_UNUSED(filename);
	Q_UNUSED(content_type);
	Q_UNUSED(transfer_encoding);
	Q_UNUSED(off);

	RequestContext *ctx = static_cast<RequestContext*>(con_cls);

	//logger::debug() << "iterate post" << key << "file:" << filename << "content type:" << content_type << "encoding:" << transfer_encoding << "offset" << int(off) << "size" << int(size);

	ctx->postdata[key].append(data, compat::castSize(size));

	return MHD_YES;
}

MHD_Result assign_to_hash(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	Q_UNUSED(kind);
	QHash<QString,QString> *hash = static_cast<QHash<QString,QString>*>(cls);
	(*hash)[QString::fromUtf8(key)] = QString::fromUtf8(value);
	return MHD_YES;
}

void request_completed(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe)
{
	Q_UNUSED(cls);
	Q_UNUSED(connection);
	Q_UNUSED(toe);

	RequestContext *ctx = static_cast<RequestContext*>(*con_cls);

	delete ctx;
	*con_cls = nullptr;
}

static void addCorsHeader(MicroHttpd::Private *d, MHD_Response *res)
{
	if(!d->allowedOrigin.isEmpty()) {
		MHD_add_response_header(
			res, "Access-Control-Allow-Origin", d->allowedOrigin.constData());
	}
}

MHD_Result request_handler(void *cls, MHD_Connection *connection, const char *url, const char *methodstr, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls)
{
	Q_UNUSED(version);
	MicroHttpd::Private *d = static_cast<MicroHttpd::Private*>(cls);

	// Create request context on first invocation
	if(!*con_cls) {
		// Check if request method is supported
		HttpRequest::Method method;
		if(qstrcmp(methodstr, "GET")==0)
			method = HttpRequest::GET;
		else if(qstrcmp(methodstr, "HEAD")==0)
			method = HttpRequest::HEAD;
		else if(qstrcmp(methodstr, "POST")==0)
			method = HttpRequest::POST;
		else if(qstrcmp(methodstr, "PUT")==0)
			method = HttpRequest::PUT;
		else if(qstrcmp(methodstr, "DELETE")==0)
			method = HttpRequest::DELETE;
		else if(qstrcmp(methodstr, "OPTIONS")==0)
			method = HttpRequest::OPTIONS;
		else {
			logMessage(connection, 405, methodstr, url);
			return MHD_NO;
		}

		// Find request handler
		HttpRequest request(method, url);
		HttpRequestHandler reqhandler;
		for(auto rhpair : d->requesthandlers) {
			QRegularExpressionMatch m = rhpair.first.match(request.path());
			if(m.hasMatch()) {
				request.setUrlMatch(m);
				reqhandler = rhpair.second;
				break;
			}
		}

		// Send 404 error immediately if no handler was found
		if(!reqhandler) {
			logMessage(connection, 404, methodstr, url);
			auto response = MHD_create_response_from_buffer(
					strlen(MSG_404),
					const_cast<char*>(MSG_404),
					MHD_RESPMEM_PERSISTENT);
			addCorsHeader(d, response);
			const auto ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
			MHD_destroy_response(response);
			return ret;
		}

		RequestContext *ctx = new RequestContext(request, reqhandler);
		*con_cls = ctx;

		// Get HTTP headers
		QHash<QString,QString> headers;
		MHD_get_connection_values(connection, MHD_HEADER_KIND, &assign_to_hash, &headers);
		ctx->request.setHeaders(headers);

		// Demand authentication if basic auth is enabled
		if(!d->baPass.isEmpty() && request.method() != HttpRequest::OPTIONS) {
			char *user, *pass = nullptr;
			bool fail = false;

			user = MHD_basic_auth_get_username_password(connection, &pass);
			if(d->baUser != user || d->baPass != pass) {
				// Invalid username or password
				logMessage(connection, 401, methodstr, url);
				fail = true;
			}

			if(user) free(user);
			if(pass) free(pass);

			if(fail) {
				auto response = MHD_create_response_from_buffer(
					strlen(MSG_401),
					const_cast<char*>(MSG_401),
					MHD_RESPMEM_PERSISTENT
					);
				addCorsHeader(d, response);

				int ret;
				// If a header indicating this was an AJAX request is set,
				// don't return the full basic auth response so the browser
				// won't pop up a password dialog.
				if(ctx->request.headers()["x-requested-with"] == "XMLHttpRequest") {
					ret = MHD_queue_response(
						connection,
						MHD_HTTP_UNAUTHORIZED,
						response);

				} else {
					ret = MHD_queue_basic_auth_fail_response(
						connection,
						d->baRealm.constData(),
						response);
				}
				MHD_destroy_response(response);
				return ret ? MHD_YES : MHD_NO;
			}
		}

		// Get GET arguments
		QHash<QString,QString> getdata;
		MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, &assign_to_hash, &getdata);
		ctx->request.setGetData(getdata);

		// Prepare POST argument processor
		if(method == HttpRequest::POST) {
			QString contenttype = ctx->request.headers()["content-type"];
			if(contenttype.startsWith("application/x-www-form-urlencoded")) {
				ctx->postprocessor = MHD_create_post_processor(connection, 32*1024, iterate_post, ctx);
				if(!ctx->postprocessor) {
					delete ctx;
					logMessage(connection, 500, methodstr, url);
					return MHD_NO;
				}
			}
		}

		return MHD_YES;
	}

	RequestContext *reqctx = static_cast<RequestContext*>(*con_cls);

	// Process POST data
	if((reqctx->request.method() == HttpRequest::POST || reqctx->request.method() == HttpRequest::PUT) && *upload_data_size != 0) {
		if(reqctx->postprocessor) {
			MHD_post_process(reqctx->postprocessor, upload_data, *upload_data_size);
			*upload_data_size = 0;

			QHash<QString,QString> postdata;
			for(auto i=reqctx->postdata.constBegin();i!=reqctx->postdata.constEnd();++i)
				postdata[i.key()] = QString::fromUtf8(i.value());

			reqctx->request.setPostData(postdata);
		} else {
			// TODO limit upload size
			reqctx->request.addBodyData(upload_data, *upload_data_size);
			*upload_data_size = 0;
		}

		return MHD_YES;
	}

	// Send response
	HttpResponse response = reqctx->reqhandler(reqctx->request);
	MHD_Response *mhdresponse;

	logMessage(connection, response.code(), methodstr, url);

	mhdresponse = MHD_create_response_from_buffer(response.body().length(), const_cast<char*>(response.body().data()), MHD_RESPMEM_MUST_COPY);

	for(auto i=response.headers().constBegin();i!=response.headers().constEnd();++i) {
		MHD_add_response_header(mhdresponse, i.key().toUtf8().constData(), i.value().toUtf8().constData());
	}

	addCorsHeader(d, mhdresponse);
	const auto ret = MHD_queue_response (connection, response.code(), mhdresponse);
	MHD_destroy_response(mhdresponse);

	return ret;
}

MHD_Result access_policy(void *cls, const struct sockaddr * addr, socklen_t addrlen)
{
	Q_UNUSED(addrlen);
	MicroHttpd::Private *d = static_cast<MicroHttpd::Private*>(cls);

	QHostAddress address(addr);

	return d->acceptPolicy(address) ? MHD_YES : MHD_NO;
}

}

MicroHttpd::MicroHttpd(QObject *parent)
	: QObject(parent),
	_d(new Private)
{
}

MicroHttpd::~MicroHttpd()
{
	stop();
	delete _d;
}

void MicroHttpd::setAcceptPolicy(const AcceptPolicy &ap)
{
	_d->acceptPolicy = ap;
}

bool MicroHttpd::listen(quint16 port)
{
	Q_ASSERT(!_d->mhd);
	qInfo("Starting microhttpd on port %d", port);

	_d->mhd = MHD_start_daemon(MHD_USE_DEBUG | MHD_USE_SELECT_INTERNALLY, port,
			&access_policy, _d,
			&request_handler, _d,
			MHD_OPTION_NOTIFY_COMPLETED, request_completed, 0,
			MHD_OPTION_END);
	if(!_d->mhd) {
		qWarning("Error starting microhttpd");
		return false;
	}
	return true;
}

bool MicroHttpd::listenFd(int fd)
{
	Q_ASSERT(!_d->mhd);
	qInfo("Starting microhttpd on pre-opened socket");

	_d->mhd = MHD_start_daemon(MHD_USE_DEBUG | MHD_USE_SELECT_INTERNALLY, 0,
			&access_policy, _d,
			&request_handler, _d,
			MHD_OPTION_NOTIFY_COMPLETED, request_completed, 0,
			MHD_OPTION_LISTEN_SOCKET, fd,
			MHD_OPTION_END);
	if(!_d->mhd) {
		qWarning("Error starting microhttpd");
		return false;
	}
	return true;
}

void MicroHttpd::stop()
{
	if(_d->mhd) {
		qInfo("Stopping microhttpd");
		MHD_stop_daemon(_d->mhd);
		_d->mhd = nullptr;
	}
}

void MicroHttpd::addRequestHandler(const QRegularExpression &urlMatcher, HttpRequestHandler handler)
{
	_d->requesthandlers.append({urlMatcher, handler});
}

void MicroHttpd::addRequestHandler(const char *urlMatcher, HttpRequestHandler handler)
{
	addRequestHandler(QRegularExpression(urlMatcher), handler);
}

void MicroHttpd::setBasicAuth(const QString &realm, const QString &username, const QString &password)
{
	_d->baRealm = realm.toUtf8();
	_d->baUser = username.toUtf8();
	_d->baPass = password.toUtf8();
}

void MicroHttpd::setAllowedOrigin(const QString &allowedOrigin)
{
	_d->allowedOrigin = allowedOrigin.toUtf8();
}

QString MicroHttpd::version()
{
	return QString::fromUtf8(MHD_get_version());
}

void HttpRequest::setHeaders(const QHash<QString,QString> &data) {
	// headers are case-insensitive, so normalize them to lower case
	QHashIterator<QString,QString> i(data);
	QHash<QString, QString> newHeaders;
	while(i.hasNext()) {
		i.next();
		newHeaders[i.key().toLower()] = i.value();
	}

	_headers = newHeaders;
}

HttpResponse HttpResponse::HtmlResponse(const QString &html)
{
	HttpResponse r(200, html.toUtf8());
	r.setHeader("Content-Type", "text/html");

	return r;
}

HttpResponse HttpResponse::JsonResponse(const QJsonDocument &doc, int statuscode)
{
	HttpResponse r(statuscode, doc.toJson());
	r.setHeader("Content-Type", "application/json");
	return r;
}

HttpResponse HttpResponse::JsonErrorResponse(const QString &message, int statuscode)
{
	QJsonObject o;
	o["status"] = "error";
	o["message"] = message;
	return JsonResponse(QJsonDocument(o), statuscode);
}

//! Return a No Content response
HttpResponse HttpResponse::NoContent(int statuscode)
{
	return HttpResponse(statuscode);
}

HttpResponse HttpResponse::MethodNotAllowed(const QStringList &allow)
{
	QString methods = allow.join(",");
	HttpResponse r(405, "<html><body>405 - Method not allowed</body></html>");
	r.setHeader("Allow", methods);
	return r;
}

HttpResponse HttpResponse::NotFound()
{
	return HttpResponse(404, MSG_404);
}

HttpResponse HttpResponse::FileResponse(const QString &path, bool head)
{
	QFile f { path };
	if(!f.open(QFile::ReadOnly))
		return NotFound();

	QByteArray body;
	qint64 size;

	if(head) {
		size = f.size();

	} else {
		body = f.readAll();
		size = body.length();
	}

	HttpResponse r { 200, body };
	r.setHeader("Content-Length", QString::number(size));

	QMimeDatabase mimedb;
	QList<QMimeType> mimetypes = mimedb.mimeTypesForFileName(path);
	if(mimetypes.isEmpty())
		r.setHeader("Content-Type", "application/octet-stream");
	else
		r.setHeader("Content-Type", mimetypes.first().name());

	return r;
}

