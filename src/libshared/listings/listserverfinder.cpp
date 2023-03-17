/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "libshared/listings/listserverfinder.h"

#include <QXmlStreamReader>

namespace sessionlisting {

static QString parseHead(QXmlStreamReader &reader, const QString &metaHeaderName)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return QByteArray();
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("meta")) {
				if(reader.attributes().value("name") == metaHeaderName)
					return reader.attributes().value("content").toString();
			}
			break;
		case QXmlStreamReader::EndElement:
			if(reader.name() == QStringLiteral("head"))
				return QByteArray();
			break;
		default: break;
		}
	}
	return QByteArray();
}

// A terrible HTML parser that should just be able to parse the <head> section
// and find the <meta> tag with the given name, even when the file isn't XHTML.
static QString parseHtml(QXmlStreamReader &reader, const QString &metaHeaderName)
{
	while(!reader.atEnd()) {
		const auto tokentype = reader.readNext();
		switch(tokentype) {
		case QXmlStreamReader::Invalid:
			return QByteArray();
		case QXmlStreamReader::StartElement:
			if(reader.name() == QStringLiteral("head"))
				return parseHead(reader, metaHeaderName);
			else if(reader.name() == QStringLiteral("body"))
				return QByteArray(); // we went too far!
			break;
		default: break;
		}
	}
	return QByteArray();
}

QString findListserverLinkHtml(QIODevice *htmlFile)
{
	QXmlStreamReader reader(htmlFile);

	return parseHtml(reader, QStringLiteral("drawpile:list-server"));
}

}

