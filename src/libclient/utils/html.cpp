// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/html.h"
#include "libshared/util/qtcompat.h"

#include <QRegularExpression>

namespace htmlutils {

QString newlineToBr(const QString &input)
{
	compat::StringView in{input};
	QString out;
	int pos=0;
	int last=0;
	while((pos=input.indexOf('\n', last))>=0) {
		out.append(in.mid(last, pos-last));
		out.append("<br>");
		last=pos+1;
	}

	if(last==0)
		return input;

	out.append(in.mid(last));
	return out;
}

QString linkify(const QString &input, const QString &extra)
{
	compat::StringView in{input};

	// This regular expression is from: http://blog.mattheworiordan.com/post/13174566389/url-regular-expression-for-links-with-or-without-the
	static const QRegularExpression linkre(
		"((([A-Za-z]{3,9}:(?:\\/\\/)?)(?:[\\-;:&=\\+\\$,\\w]+@)?[A-Za-z0-9\\.\\-]+|(?:www\\.|[\\-;:&=\\+\\$,\\w]+@)[A-Za-z0-9\\.\\-]+)(?::\\d+)?((?:\\/[\\+~%\\/\\.\\w\\-_]*)?\\?" "?(?:[\\-\\+=&;%@\\.\\w_]*)(?:#[^ ]*)?)?)"
		);

	static const QRegularExpression protore("^[a-zA-Z]{3,}:");

	auto matches = linkre.globalMatch(in);
	QString out;
	int pos=0;

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
	if(pos==0)
		return input;

	out.append(in.mid(pos));

	return out;
}

QString wrapEmoji(const QString &input, const QString &replacement)
{
	// This regex is taken from the Unicode Technical Standard #51 version 15,
	// revision 23, section 1.4.9 "EBNF and Regex". It doesn't exactly match
	// emoji, but it should hopefully be close enough for our use case.
	// https://unicode.org/reports/tr51/#EBNF_and_Regex
	static QRegularExpression re{
		"(\\p{RI}\\p{RI}|\\p{Emoji}(\\p{EMod}|\\x{FE0F}\\x{20E3}?|[\\x{E0020}-"
		"\\x{E007E}]+\\x{E007F})?(\\x{200D}(\\p{RI}\\p{RI}|\\p{Emoji}(\\p{EMod}"
		"|\\x{FE0F}\\x{20E3}?|[\\x{E0020}-\\x{E007E}]+\\x{E007F})?))*)+"};
	QString s = input;
	s.replace(re, replacement);
	return s;
}

}
