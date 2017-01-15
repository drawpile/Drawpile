/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "netfiles.h"
#include "widgets/netstatus.h"
#include "../shared/util/networkaccess.h"

#include <QNetworkReply>
#include <QDebug>
#include <QApplication>

#include <QTemporaryFile>
#include <QDir>
#include <QImageReader>

namespace networkaccess {

void getFile(const QUrl &url, const QString &expectType, widgets::NetStatus *netstatus, std::function<void (QFile &, const QString &)> callback)
{
	QString fileExt = ".tmp";
	QString filename = url.path();
	int fileExtIndex = filename.lastIndexOf('.');
	if(fileExtIndex>0)
		fileExt = filename.mid(fileExtIndex);

	QTemporaryFile *tempfile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/drawpile_XXXXXX-download") + fileExt);
	if(!tempfile->open()) {
		callback(*tempfile, tempfile->errorString());
		delete tempfile;
		return;
	}

	QNetworkReply *reply = get(url, expectType);

	if(netstatus) {
		netstatus->connect(reply, &QNetworkReply::downloadProgress, netstatus, &widgets::NetStatus::setDownloadProgress);
		netstatus->connect(reply, &QNetworkReply::finished, netstatus, &widgets::NetStatus::NetStatus::hideDownloadProgress);
	}

	reply->connect(reply, &QNetworkReply::readyRead, [reply, tempfile]() {
		tempfile->write(reply->readAll());
	});

	reply->connect(reply, &QNetworkReply::finished, [reply, expectType, callback, tempfile]() {
		QString errormsg;

		if(reply->error()) {
			if(reply->error() == QNetworkReply::OperationCanceledError &&
				!reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("image/")) {
				qWarning() << reply->url() << "expected" << expectType << ", got" << reply->header(QNetworkRequest::ContentTypeHeader);
				errormsg = QApplication::tr("Unexpected file format");
			} else {
				errormsg = reply->errorString();
			}

		}

		tempfile->seek(0);
		callback(*tempfile, errormsg);
		delete tempfile;
		reply->deleteLater();
	});

}

void getImage(const QUrl &url, widgets::NetStatus *netstatus, std::function<void (const QImage &, const QString &)> callback)
{
	getFile(url, "image/", netstatus, [callback](QFile &file, const QString &error) {
		if(!error.isEmpty()) {
			callback(QImage(), error);
		} else {
			QImageReader reader(file.fileName());
			QImage image = reader.read();

			if(image.isNull())
				callback(image, reader.errorString());
			else
				callback(image, QString());
		}
	});
}

}
