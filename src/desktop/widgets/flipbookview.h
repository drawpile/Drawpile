/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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
#ifndef FLIPBOOKVIEW_H
#define FLIPBOOKVIEW_H

#include <QWidget>

class QRubberBand;

class FlipbookView : public QWidget
{
	Q_OBJECT
public:
	explicit FlipbookView(QWidget *parent = nullptr);

public slots:
	void setPixmap(const QPixmap &pixmap);
	void startCrop();

signals:
	/**
	 * @brief Crop region selected
	 * @param rect normalized selection region (values in range [0.0..1.0])
	 */
	void cropped(const QRectF &rect);

protected:
	void paintEvent(QPaintEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

private:
	QPixmap m_pixmap;
	QRubberBand *m_rubberband;
	QPoint m_cropStart;
};

#endif // FLIPBOOKVIEW_H
