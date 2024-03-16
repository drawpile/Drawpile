#include "libclient/utils/annotations.h"
#include <QPainter>
#include <QTextCursor>
#include <QTextDocument>

namespace utils {

void setAliasedAnnotationFonts(QTextDocument *doc, bool alias)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::Start);
	cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	QTextCharFormat fmt;
	fmt.setFontStyleStrategy(alias ? QFont::NoAntialias : QFont::PreferDefault);
	cursor.mergeCharFormat(fmt);
}

void paintAnnotation(
	QPainter *painter, const QSize &size, const QColor &background,
	const QString &text, bool alias, int valign)
{
	QRectF rect(QPointF(), size);
	painter->fillRect(rect, background);
	QTextDocument doc;
	doc.setHtml(text);
	paintAnnotationContent(painter, doc, alias, valign, rect, true);
}

void paintAnnotationContent(
	QPainter *painter, QTextDocument &doc, bool alias, int valign,
	const QRectF &rect, bool updateAliasedFonts)
{
	doc.setTextWidth(rect.width());
	if(updateAliasedFonts) {
		setAliasedAnnotationFonts(&doc, alias);
	}

	QPointF offset;
	switch(valign) {
	case 1:
		offset.setY((rect.height() - doc.size().height()) / 2);
		break;
	case 2:
		offset.setY(rect.height() - doc.size().height());
		break;
	default:
		break;
	}

	painter->translate(rect.topLeft() + offset);
	painter->setRenderHint(QPainter::TextAntialiasing, !alias);
	doc.drawContents(painter, QRectF(-offset, rect.size()));
}

}
