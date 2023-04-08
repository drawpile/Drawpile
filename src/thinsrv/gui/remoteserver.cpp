// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/remoteserver.h"
#include "libshared/util/networkaccess.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace server{
namespace gui {

RemoteServer::RemoteServer(const QUrl &url, QObject *parent)
	: Server(parent), m_baseurl(url)
{

}

void RemoteServer::makeApiRequest(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &requestBody)
{
	QNetworkAccessManager *net = networkaccess::getInstance();

	QString urlPath = m_baseurl.path();
	if(urlPath.isEmpty() || urlPath.at(urlPath.size()-1) != '/')
		urlPath.append('/');
	urlPath.append(path.join('/'));

	QUrl requestUrl = m_baseurl;
	requestUrl.setPath(urlPath);

	if(method == JsonApiMethod::Get && !requestBody.isEmpty()) {
		QUrlQuery query;
		for(auto i=requestBody.begin();i!=requestBody.end();++i) {
			query.addQueryItem(i.key(), i.value().toVariant().toString());
		}
		requestUrl.setQuery(query);
	}

	QNetworkRequest req(requestUrl);

	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply *reply=nullptr;
	switch(method) {
	case JsonApiMethod::Get: reply = net->get(req); break;
	case JsonApiMethod::Create: reply = net->post(req, QJsonDocument(requestBody).toJson()); break;
	case JsonApiMethod::Update: reply = net->put(req, QJsonDocument(requestBody).toJson()); break;
	case JsonApiMethod::Delete: reply = net->deleteResource(req); break;
	}
	Q_ASSERT(reply);

	connect(reply, &QNetworkReply::finished, reply, [this, reply, requestId]() {
		JsonApiResult::Status status;

		if(reply->error() == QNetworkReply::NoError) {
			status = JsonApiResult::Ok;

		} else if(reply->error() == QNetworkReply::AuthenticationRequiredError) {
			status = JsonApiResult::ConnectionError;
			/* todo */

		} else {
			switch(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()) {
			case 400: status = JsonApiResult::BadRequest; break;
			case 404: status = JsonApiResult::NotFound; break;
			case 500: status = JsonApiResult::InternalError; break;
			default: status = JsonApiResult::ConnectionError; break;
			}
		}

		QJsonDocument doc;
		if(status == JsonApiResult::ConnectionError) {
			QJsonObject err;
			err["status"] = "error";
			err["message"] = reply->errorString();
			doc = QJsonDocument(err);

		} else {
			doc = QJsonDocument::fromJson(reply->readAll());
		}

		onApiResponse(requestId, JsonApiResult { status, doc });

		reply->deleteLater();
	});
}

}
}
