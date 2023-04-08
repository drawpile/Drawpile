// SPDX-License-Identifier: GPL-3.0-or-later

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

