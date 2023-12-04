#include "libclient/utils/annotations.h"
#include <QPainter>
#include <QTextDocument>

namespace utils {

void paintAnnotation(
	QPainter *painter, const QSize &size, const QColor &background,
	const QString &text, int valign)
{
	QRectF rect(QPointF(), size);
	painter->fillRect(rect, background);

	QTextDocument doc;
	doc.setHtml(text);
	doc.setTextWidth(rect.width());

	QPointF offset;
	switch(valign) {
	case 1:
		offset.setY((rect.height() - doc.size().height()) / 2);
		break;
	case 2:
		offset.setY(rect.height() - doc.size().height());
		break;
	}
	painter->translate(offset);

	doc.drawContents(painter, QRectF(-offset, rect.size()));
}

}
