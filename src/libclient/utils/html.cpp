// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/html.h"
#include "libshared/util/qtcompat.h"

#include <QRegularExpression>

namespace htmlutils {

QString newlineToBr(const QString &input)
{
	compat::StringView in(input);
	QString out;
	int pos = 0;
	int last = 0;
	while((pos = input.indexOf('\n', last)) >= 0) {
		out.append(in.mid(last, pos - last));
		out.append("<br>");
		last = pos + 1;
	}

	if(last == 0) {
		return input;
	}

	out.append(in.mid(last));
	return out;
}

QString linkify(const QString &input, const QString &extra)
{
	compat::StringView in(input);

	// This regular expression is from:
	// https://blog.mattheworiordan.com/post/13174566389/
	static const QRegularExpression linkre(
		"((([A-Za-z]{3,9}:(?:\\/\\/"
		")?)(?:[\\-;:&=\\+\\$,\\w]+@)?[A-Za-z0-9\\.\\-]+|(?:www\\.|[\\-;:&=\\+"
		"\\$,\\w]+@)[A-Za-z0-9\\.\\-]+)(?::\\d+)?((?:\\/[\\+~%\\/"
		"\\.\\w\\-_@]*)?\\?"
		"?(?:[\\-\\+=&;%@\\.\\w_]*)(?:#[^ ]*)?)?)");

	static const QRegularExpression protore("^[a-zA-Z]{3,}:");

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
	QRegularExpressionMatchIterator matches = linkre.globalMatchView(in);
#else
	QRegularExpressionMatchIterator matches = linkre.globalMatch(in);
#endif
	QString out;
	int pos = 0;

	while(matches.hasNext()) {
		auto m = matches.next();
		QString url = m.captured();
		out.append(in.mid(pos, m.capturedStart() - pos));
		pos = m.capturedEnd();

		out.append("<a href=\"");
		if(!protore.match(url).hasMatch())
			out.append("http://");
		out.append(url);
		out.append('"');
		if(!extra.isEmpty()) {
			out.append(' ');
			out.append(extra);
		}
		out.append('>');
		out.append(url);
		out.append("</a>");
	}

	// special case optimization: no matches
	if(pos == 0) {
		return input;
	}

	out.append(in.mid(pos));

	return out;
}

}
