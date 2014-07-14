/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "networkaccess.h"
#include "widgets/netstatus.h"

#include <QNetworkReply>
#include <QDebug>
#include <QApplication>
#include <QMimeDatabase>

namespace networkaccess {

QNetworkAccessManager *getInstance()
{
	static QNetworkAccessManager *instance;
	if(!instance)
		instance = new QNetworkAccessManager;

	return instance;
}

QNetworkReply *get(const QUrl &url, const QString &expectType, widgets::NetStatus *netstatus)
{
	QNetworkRequest req(url);

	QNetworkReply *reply = getInstance()->get(req);
	if(netstatus) {
		netstatus->connect(reply, SIGNAL(downloadProgress(qint64,qint64)), netstatus, SLOT(bytesReceived(qint64,qint64)));
		netstatus->connect(reply, &QNetworkReply::finished, [netstatus]() { netstatus->bytesReceived(0,0); });
	}

	reply->connect(reply, &QNetworkReply::metaDataChanged, [reply, expectType]() {
		QVariant mimetype = reply->header(QNetworkRequest::ContentTypeHeader);
		if(mimetype.isValid()) {
			if(mimetype.toString().startsWith(expectType)==false)
				reply->close();
		}
	});
	return reply;
}

void getImage(const QUrl &url, widgets::NetStatus *netstatus, std::function<void (const QImage &, const QString &)> callback)
{
	QNetworkReply *reply = get(url, "image/", netstatus);

	reply->connect(reply, &QNetworkReply::finished, [reply, callback]() {
		QImage img;
		QString errormsg;

		if(reply->error()) {
			if(reply->error() == QNetworkReply::OperationCanceledError &&
				!reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("image/")) {
				qWarning() << reply->url() << "expected image, got" << reply->header(QNetworkRequest::ContentTypeHeader);
				errormsg = QApplication::tr("Unexpected file format");
			} else {
				errormsg = reply->errorString();
			}

		} else {
			QMimeDatabase db;
			QByteArray imgdata = reply->readAll();
			QMimeType mimetype = db.mimeTypeForData(imgdata);

			if(!mimetype.name().startsWith("image/"))
				mimetype = db.mimeTypeForUrl(reply->url());

			img = QImage::fromData(imgdata, mimetype.preferredSuffix().toLocal8Bit().constData());
			if(img.isNull())
				errormsg = QApplication::tr("The image could not be loaded");
		}

		callback(img, errormsg);
		reply->deleteLater();
	});
}

}
