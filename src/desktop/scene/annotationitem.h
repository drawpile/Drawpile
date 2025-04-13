// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_ANNOTATIONITEM_H
#define DESKTOP_SCENE_ANNOTATIONITEM_H
#include "desktop/scene/baseitem.h"
#include <QPixmap>
#include <QTextDocument>

namespace drawingboard {

/**
 * @brief A text box that can be overlaid on the picture.
 */
class AnnotationItem final : public BaseItem {
public:
	enum { Type = UserType + 10 };

	AnnotationItem(int id, qreal zoom, QGraphicsItem *parent = nullptr);

	//! Get the ID number of this annotation
	int id() const { return m_id; }

	//! Get the user part of the ID
	int userId() const;

	//! Set the text box position and size
	void setGeometry(const QRect &rect);

	QRectF rect() const { return m_rect; }

	//! Set the background color (transparent by default)
	void setColor(const QColor &color);

	QColor color() const { return m_color; }

	//! Set the text content (subset of HTML is supported)
	void setText(const QString &text);

	QString text() const { return m_doc.toHtml(); }

	//! Set vertical alignment
	void setValign(int valign);

	int valign() const { return m_valign; }

	//! Set the "protected" flag
	void setProtect(bool protect) { m_protect = protect; }
	bool protect() const { return m_protect; }

	void setAlias(bool alias);
	bool alias() const { return m_alias; }

	void setRasterize(bool rasterize);
	bool rasterize() const { return m_rasterize; }

	//! Highlight this item
	void setHighlight(bool h);

	void setZoom(qreal zoom);

	//! Enable border
	void setShowBorder(bool show);

	QRectF boundingRect() const override;

	int type() const override { return Type; }

	//! Render this annotation into an image
	QImage toImage() const;

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *) override;

private:
	static constexpr int HANDLE_SIZE = 10;

	static int calculateHandleSize(qreal zoom);
	void paintHiddenBorder(QPainter *painter);
	const QPixmap &renderPixmap();

	int m_id;
	int m_valign = 0;
	int m_handleSize;
	QRectF m_rect;
	QColor m_color = Qt::transparent;
	QTextDocument m_doc;
	QPixmap m_pixmap;

	bool m_alias = false;
	bool m_aliasDirty = true;
	bool m_rasterize = false;
	bool m_pixmapDirty = true;
	bool m_highlight = false;
	bool m_showborder = false;
	bool m_protect = false;
};

}

#endif
