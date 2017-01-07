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

#include "networkaccess.h"

#include <QNetworkReply>
#include <QMimeDatabase>
#include <QMutexLocker>
#include <QHash>
#include <QThread>

namespace networkaccess {

struct Managers {
	QMutex mutex;
	QHash<void*, QNetworkAccessManager*> perThreadManagers;
};

static Managers MANAGERS;

QNetworkAccessManager *getInstance()
{
	QMutexLocker lock(&MANAGERS.mutex);

	QThread *t = QThread::currentThread();

	QNetworkAccessManager *nam = MANAGERS.perThreadManagers[t];
	if(!nam) {
		qDebug() << "Creating new NetworkAccessManager for thread" << t;
		nam = new QNetworkAccessManager;
		t->connect(t, &QThread::finished, [nam, t]() {
			qDebug() << "thread" << t << "ended. Removing NetworkAccessManager";
			nam->deleteLater();
			QMutexLocker lock(&MANAGERS.mutex);
			MANAGERS.perThreadManagers.remove(t);
		});
		MANAGERS.perThreadManagers[t] = nam;
	}
	return nam;
}

QNetworkReply *get(const QUrl &url, const QString &expectType)
{
	QNetworkRequest req(url);

	QNetworkReply *reply = getInstance()->get(req);

	reply->connect(reply, &QNetworkReply::metaDataChanged, [reply, expectType]() {
		QVariant mimetype = reply->header(QNetworkRequest::ContentTypeHeader);
		if(mimetype.isValid()) {
			if(mimetype.toString().startsWith(expectType)==false)
				reply->close();
		}
	});
	return reply;
}

}
