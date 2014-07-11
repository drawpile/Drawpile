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

#include "staticfileserver.h"
#include "../shared/util/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMimeDatabase>

StaticFileServer::StaticFileServer(const QString &root)
{
	QDir d(root);
	_root = d.absolutePath();
}

HttpResponse StaticFileServer::operator()(const HttpRequest &req)
{
	if(req.method() != HttpRequest::GET && req.method() != HttpRequest::HEAD)
		return HttpResponse::MethodNotAllowed(QStringList() << "HEAD" << "GET");

	if(_root.isEmpty())
		return HttpResponse::NotFound();

	QString path = QDir::cleanPath(_root + req.pathMatch().captured(1));
	if(!path.startsWith(_root))
		return HttpResponse::NotFound();

	if(QFileInfo(path).isDir())
		path.append("/index.html");

	QFile file(path);
	if(!file.open(QFile::ReadOnly)) {
		logger::warning() << path << file.errorString();
		return HttpResponse::NotFound();
	}

	// TODO handle HEAD type request
	HttpResponse resp(200);

	QMimeDatabase mimedb;
	QList<QMimeType> mimetypes = mimedb.mimeTypesForFileName(path);

	if(mimetypes.isEmpty())
		resp.setHeader("Content-Type", "application/octet-stream");
	else
		resp.setHeader("Content-Type", mimetypes.first().name());


	if(req.method() == HttpRequest::GET)
		resp.setBody(file.readAll());
	else
		resp.setHeader("Content-Length", QString::number(file.size()));

	file.close();
	return resp;
}
