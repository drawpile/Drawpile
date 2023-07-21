// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PALETTEWIDGET_H
#define PALETTEWIDGET_H

#include <QPointer>
#include <QWidget>
#include <QtColorWidgets/color_palette.hpp>

class QMenu;
class QRubberBand;
class QScrollBar;

namespace color_widgets {
class ColorDialog;
}

namespace widgets {

class PaletteWidget final : public QWidget {
	Q_OBJECT
public:
	static constexpr int MAX_COLUMNS = 32;

	PaletteWidget(QWidget *parent = nullptr);

	void setColorPalette(const color_widgets::ColorPalette &palette);
	color_widgets::ColorPalette &colorPalette() { return m_colorPalette; }
	const color_widgets::ColorPalette &colorPalette() const
	{
		return m_colorPalette;
	}

	void setSpacing(int spacing);

	int columns() const { return m_columns; }
	void setColumns(int columns);

	void setMaxRows(int maxRows);
	void setEnableScrolling(bool enable);

	void setNextColor(const QColor &color) { m_nextColor = color; }

signals:
	void colorSelected(const QColor &color);
	void columnsChanged(int columns);

protected:
	bool event(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *) override;

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;
	void wheelEvent(QWheelEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

	void focusInEvent(QFocusEvent *) override;
	void focusOutEvent(QFocusEvent *) override;

private slots:
	void scroll(int pos);
	void addColor();
	void removeColor();
	void editCurrentColor();
	void setCurrentColor(const QColor &color);
	void dialogDone();

private:
	static constexpr long long MIN_DRAG_TIME_MSEC = 200;

	int indexAt(const QPoint &point, bool extraPadding = false) const;
	int nearestAt(const QPoint &point) const;
	QRect swatchRect(int index) const;
	QRect betweenRect(int index) const;
	QSize calcSwatchSize(int availableWidth) const;

	color_widgets::ColorPalette m_colorPalette;
	QScrollBar *m_scrollbar;
	color_widgets::ColorDialog *m_colordlg;
	QMenu *m_contextmenu;

	QSize m_swatchsize;
	int m_columns;
	int m_spacing;
	int m_leftMargin;

	int m_scroll;
	int m_selection;
	int m_dialogsel;
	int m_maxrows;
	bool m_enableScrolling;
	QPoint m_dragstart;
	long long m_dragTime;
	QRubberBand *m_outline;
	QColor m_nextColor;
};

}

#endif
