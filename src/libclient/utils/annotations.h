// SPDX-License-Identifier: GPL-3.0-or-later
class QColor;
class QPainter;
class QRectF;
class QSize;
class QString;
class QTextDocument;

namespace utils {

void setAliasedAnnotationFonts(QTextDocument *doc, bool alias);

// Changes the state of the given painter, save it if you want to keep using it!
void paintAnnotation(
	QPainter *painter, const QSize &size, const QColor &background,
	const QString &text, bool alias, int valign);

// Changes the state of the given painter, save it if you want to keep using it!
// Changes the text width of the given document and, if updateAliasedFonts is
// passed, also its font style strategies.
void paintAnnotationContent(
	QPainter *painter, QTextDocument &doc, bool alias, int valign,
	const QRectF &rect, bool updateAliasedFonts);

}
