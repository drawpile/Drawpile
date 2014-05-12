/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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
#ifndef PALETTEWIDGET_H
#define PALETTEWIDGET_H

#include <QWidget>

class Palette;
class QScrollBar;
class QRubberBand;
class QMenu;

namespace dialogs {

	class ColorDialog;

}

namespace widgets {

class PaletteWidget : public QWidget {
	Q_OBJECT
public:
	PaletteWidget(QWidget *parent);

	void setPalette(Palette *palette);

	void setSpacing(int spacing);

signals:
	void colorSelected(const QColor& color);

protected:
	bool event(QEvent *event);
	void resizeEvent(QResizeEvent *event);
	void paintEvent(QPaintEvent *);

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *);
	void wheelEvent(QWheelEvent *event);
	void contextMenuEvent(QContextMenuEvent *event);
	void keyReleaseEvent(QKeyEvent *event);

	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dragLeaveEvent(QDragLeaveEvent *event);
	void dropEvent(QDropEvent *event);

	void focusInEvent(QFocusEvent*);
	void focusOutEvent(QFocusEvent*);

private slots:
	void scroll(int pos);
	void addColor();
	void removeColor();
	void editCurrentColor();
	void setCurrentColor(const QColor& color);
	void dialogDone();

private:
	int indexAt(const QPoint& point, bool extraPadding=false) const;
	int nearestAt(const QPoint& point) const;
	QRect swatchRect(int index) const;
	QRect betweenRect(int index) const;
	QSize calcSwatchSize(int availableWidth) const;

	Palette *_palette;
	QScrollBar *_scrollbar;
	dialogs::ColorDialog *_colordlg;
	QMenu *_contextmenu;

	QSize _swatchsize;
	int _columns;
	int _spacing;
	int _leftMargin;

	int _scroll;
	int _selection;
	int _dialogsel;
	QPoint _dragstart;
	QRubberBand *_outline;
};

}

#endif

