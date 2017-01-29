/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "qmhttp.h"

#include <QList>
#include <QPair>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <microhttpd.h>

struct MicroHttpd::Private {
	MHD_Daemon *mhd;
	QList<QPair<QRegularExpression, HttpRequestHandler>> requesthandlers;

	// Basic auth
	QString baRealm;
	QString baUser;
	QString baPass;

	// Access controls
	AcceptPolicy acceptPolicy;

	// standard error responses
	MHD_Response *response404;
	MHD_Response *response401;

	Private() : mhd(0)
	{
		acceptPolicy = [](const QHostAddress &addr) { return addr.isLoopback(); };

		const char *msg404 = "<html><body>404 - Page not found</body></html>";
		response404 = MHD_create_response_from_buffer(strlen(msg404), const_cast<char*>(msg404), MHD_RESPMEM_PERSISTENT);
		const char *msg401 = "<html><body>401 - Unauthorized</body></html>";
		response401 = MHD_create_response_from_buffer(strlen(msg401), const_cast<char*>(msg401), MHD_RESPMEM_PERSISTENT);
	}

	~Private()
	{
		MHD_destroy_response(response404);
		MHD_destroy_response(response401);
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

int iterate_post(void *con_cls, enum MHD_ValueKind kind, const char *key, const char *filename, const char *content_type, const char *transfer_encoding, const char *data, uint64_t off, size_t size)
{
	Q_UNUSED(kind);
	Q_UNUSED(filename);
	Q_UNUSED(content_type);
	Q_UNUSED(transfer_encoding);
	Q_UNUSED(off);

	RequestContext *ctx = reinterpret_cast<RequestContext*>(con_cls);

	//logger::debug() << "iterate post" << key << "file:" << filename << "content type:" << content_type << "encoding:" << transfer_encoding << "offset" << int(off) << "size" << int(size);

	ctx->postdata[key].append(data, size);

	return MHD_YES;
}

int assign_to_hash(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	Q_UNUSED(kind);
	QHash<QString,QString> *hash = reinterpret_cast<QHash<QString,QString>*>(cls);
	(*hash)[QString::fromUtf8(key)] = QString::fromUtf8(value);
	return MHD_YES;
}

void request_completed(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe)
{
	Q_UNUSED(cls);
	Q_UNUSED(connection);
	Q_UNUSED(toe);

	RequestContext *ctx = reinterpret_cast<RequestContext*>(*con_cls);

	delete ctx;
	*con_cls = nullptr;
}

int request_handler(void *cls, MHD_Connection *connection, const char *url, const char *methodstr, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls)
{
	Q_UNUSED(version);
	MicroHttpd::Private *d = reinterpret_cast<MicroHttpd::Private*>(cls);

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
		else
			return MHD_NO;

		// Demand authentication if basic auth is enabled
		if(!d->baPass.isEmpty()) {
			char *user, *pass;
			user = MHD_basic_auth_get_username_password(connection, &pass);
			if(user != d->baUser || pass != d->baPass) {
				// Invalid username or password
				return MHD_queue_basic_auth_fail_response(connection, d->baRealm.toUtf8().constData(), d->response401);
			}

			if(user) free(user);
			if(pass) free(pass);
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
			return MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, d->response404);
		}

		RequestContext *ctx = new RequestContext(request, reqhandler);
		*con_cls = ctx;

		// Get HTTP headers
		QHash<QString,QString> headers;
		MHD_get_connection_values(connection, MHD_HEADER_KIND, &assign_to_hash, &headers);
		ctx->request.setHeaders(headers);
		
		// Get GET arguments
		QHash<QString,QString> getdata;
		MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, &assign_to_hash, &getdata);
		ctx->request.setGetData(getdata);

		// Prepare POST argument processor
		if(method == HttpRequest::POST) {
			QString contenttype = ctx->request.headers()["Content-Type"];
			if(contenttype.startsWith("application/x-www-form-urlencoded")) {
				ctx->postprocessor = MHD_create_post_processor(connection, 32*1024, iterate_post, ctx);
				if(!ctx->postprocessor) {
					delete ctx;
					return MHD_NO;
				}
			}
		}

		return MHD_YES;
	}

	RequestContext *reqctx = reinterpret_cast<RequestContext*>(*con_cls);

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
	int ret;

	mhdresponse = MHD_create_response_from_buffer(response.body().length(), const_cast<char*>(response.body().data()), MHD_RESPMEM_MUST_COPY);

	for(auto i=response.headers().constBegin();i!=response.headers().constEnd();++i) {
		MHD_add_response_header(mhdresponse, i.key().toUtf8().constData(), i.value().toUtf8().constData());
	}

	ret = MHD_queue_response (connection, response.code(), mhdresponse);
	MHD_destroy_response(mhdresponse);

	return ret;
}

int access_policy(void *cls, const struct sockaddr * addr, socklen_t addrlen)
{
	Q_UNUSED(addrlen);
	MicroHttpd::Private *d = reinterpret_cast<MicroHttpd::Private*>(cls);

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

void MicroHttpd::stop()
{
	if(_d->mhd) {
		qInfo("Stopping microhttpd");
		MHD_stop_daemon(_d->mhd);
		_d->mhd = 0;
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
	_d->baRealm = realm;
	_d->baUser = username;
	_d->baPass = password;
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

HttpResponse HttpResponse::MethodNotAllowed(const QStringList &allow)
{
	QString methods = allow.join(",");
	HttpResponse r(405, "<html><body>405 - Method not allowed</body></html>");
	r.setHeader("Allow", methods);
	return r;
}

HttpResponse HttpResponse::NotFound()
{
	return HttpResponse(404, "<html><body>404 - Page not found</body></html>");
}

