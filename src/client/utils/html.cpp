/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "html.h"

#include <QRegularExpression>

namespace htmlutils {

QString newlineToBr(const QString &input)
{
	QString out;
	int pos=0;
	int last=0;
	while((pos=input.indexOf('\n', last))>=0) {
		out.append(input.midRef(last, pos-last));
		out.append("<br>");
		last=pos+1;
	}

	if(last==0)
		return input;

	out.append(input.midRef(last));
	return out;
}

QString linkify(const QString &input)
{
	// This regular expression is from: http://blog.mattheworiordan.com/post/13174566389/url-regular-expression-for-links-with-or-without-the
	static const QRegularExpression linkre(
		"((([A-Za-z]{3,9}:(?:\\/\\/)?)(?:[\\-;:&=\\+\\$,\\w]+@)?[A-Za-z0-9\\.\\-]+|(?:www\\.|[\\-;:&=\\+\\$,\\w]+@)[A-Za-z0-9\\.\\-]+)((?:\\/[\\+~%\\/\\.\\w\\-_]*)?\\?" "?(?:[\\-\\+=&;%@\\.\\w_]*)#?(?:[\\.\\!\\/\\\\\\w]*))?)"
		);

	static const QRegularExpression protore("^[a-zA-Z]{3,}:");

	auto matches = linkre.globalMatch(input);
	QString out;
	int pos=0;

	while(matches.hasNext()) {
		auto m = matches.next();
		QString url = m.captured();
		out.append(input.midRef(pos, m.capturedStart() - pos));
		pos = m.capturedEnd();

		out.append("<a href=\"");
		if(!protore.match(url).hasMatch())
			out.append("http://");
		out.append(url);
		out.append("\">");
		out.append(url);
		out.append("</a>");
	}

	// special case optimization: no matches
	if(pos==0)
		return input;

	out.append(input.midRef(pos));

	return out;
}

}
