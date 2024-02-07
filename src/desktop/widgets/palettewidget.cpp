// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/palettewidget.h"
#include "desktop/dialogs/colordialog.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDrag>
#include <QHelpEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QScrollBar>
#include <QToolTip>
#include <desktop/utils/qtguicompat.h>

namespace widgets {

PaletteWidget::PaletteWidget(QWidget *parent)
	: QWidget(parent)
	, m_colorPalette(QVector<QColor>{}, QString{}, 16)
	, m_swatchsize(13, 8)
	, m_columns(16)
	, m_spacing(1)
	, m_leftMargin(0)
	, m_scroll(0)
	, m_selection(-1)
	, m_dialogsel(-2)
	, m_maxrows(0)
	, m_enableScrolling(true)
	, m_dragTime(0)
	, m_nextColor(Qt::black)
{
	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	m_outline = new QRubberBand(QRubberBand::Rectangle, this);

	m_contextmenu = new QMenu(this);
	m_contextmenu->addAction(tr("Add"), this, SLOT(addColor()));
	m_contextmenu->addAction(tr("Modify"), this, SLOT(editCurrentColor()));
	m_contextmenu->addAction(tr("Remove"), this, SLOT(removeColor()));

	m_scrollbar = new QScrollBar(this);
	connect(m_scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scroll(int)));

	m_colordlg = new color_widgets::ColorDialog(this);
	m_colordlg->setWindowTitle(tr("Set palette color"));

	connect(
		m_colordlg, SIGNAL(colorSelected(QColor)), this,
		SLOT(setCurrentColor(QColor)));
	connect(
		m_colordlg, &color_widgets::ColorDialog::colorSelected, this,
		&PaletteWidget::colorSelected);

	connect(m_colordlg, SIGNAL(finished(int)), this, SLOT(dialogDone()));

	connect(
		&m_colorPalette, &color_widgets::ColorPalette::colorsChanged, this,
		QOverload<>::of(&QWidget::update));
	connect(
		&m_colorPalette, &color_widgets::ColorPalette::colorsUpdated, this,
		QOverload<>::of(&QWidget::update));
}

void PaletteWidget::setMaxRows(int maxRows)
{
	m_maxrows = maxRows;
}

void PaletteWidget::setColumns(int columns)
{
	m_columns = qBound(1, columns, MAX_COLUMNS);
	m_colorPalette.setColumns(columns);
	resizeEvent(nullptr);
	update();
	emit columnsChanged(m_columns);
}

void PaletteWidget::setEnableScrolling(bool enable)
{
	m_enableScrolling = enable;
}

void PaletteWidget::setColorPalette(const color_widgets::ColorPalette &palette)
{

	m_colorPalette = palette;
	m_selection = -1;
	m_dialogsel = -2;
	setColumns(m_colorPalette.columns());
}

void PaletteWidget::setSpacing(int spacing)
{
	Q_ASSERT(spacing >= 0);
	m_spacing = spacing;
	resizeEvent(nullptr);
	update();
}

QSize PaletteWidget::calcSwatchSize(int availableWidth) const
{
	QSize s;
	s.setWidth(qMax(
		1 + m_spacing * 2,
		(availableWidth - m_spacing) / m_columns - m_spacing));
	s.setHeight(qMin(int(s.width() * 0.75 + 0.5), availableWidth / 16));
	return s;
}

namespace {
int divRoundUp(int a, int b)
{
	return a / b + (a % b ? 1 : 0);
}
}
void PaletteWidget::resizeEvent(QResizeEvent *)
{
	int rowsHeight = 0;

	int contentWidth = width();

	int count = 0;

	// First calculate required space without scrollbar
	count = m_colorPalette.count() + 1;
	m_swatchsize = calcSwatchSize(contentWidth);
	rowsHeight = (count / m_columns + 1) * (m_swatchsize.height() + m_spacing);

	if(rowsHeight <= height() || !m_enableScrolling) {
		m_scrollbar->setVisible(false);
		m_scrollbar->setMaximum(0);

	} else {
		// Recalculate width taking scrollbar in account
		contentWidth -= m_scrollbar->sizeHint().width();
		m_swatchsize = calcSwatchSize(contentWidth);
		rowsHeight =
			divRoundUp(count, m_columns) * (m_swatchsize.height() + m_spacing);

		m_scrollbar->setGeometry(
			QRect(contentWidth, 0, m_scrollbar->sizeHint().width(), height()));

		m_scrollbar->setMaximum(rowsHeight - this->height() + 1);
		m_scrollbar->setPageStep(height());
		m_scrollbar->setVisible(true);
	}

	m_scrollbar->setMinimum(0);

	m_leftMargin = (contentWidth - m_spacing -
					(m_swatchsize.width() + m_spacing) * m_columns) /
				   2;

	if(m_outline->isVisible() && m_selection >= 0)
		m_outline->setGeometry(swatchRect(m_selection).adjusted(-1, -1, 1, 1));
}

void PaletteWidget::scroll(int pos)
{
	m_scroll = pos;
	if(m_outline->isVisible() && m_selection >= 0)
		m_outline->setGeometry(swatchRect(m_selection).adjusted(-1, -1, 1, 1));

	update();
}

/**
 * Pop up a dialog to add a new color
 */
void PaletteWidget::addColor()
{
	if(m_dialogsel < -1) {
		m_dialogsel = -1;
		dialogs::applyColorDialogSettings(m_colordlg);
		m_colordlg->setColor(m_nextColor);
		m_colordlg->show();
	}
}

void PaletteWidget::removeColor()
{
	m_colorPalette.eraseColor(m_selection);
	if(m_selection >= m_colorPalette.count()) {
		m_outline->hide();
		m_selection = -1;
	}
	update();
}

/**
 * Pop up a dialog to edit the currently selected color
 */
void PaletteWidget::editCurrentColor()
{
	if(m_dialogsel < -1 && m_selection >= 0) {
		m_dialogsel = m_selection;
		m_colordlg->setColor(m_colorPalette.colorAt(m_selection));
		m_colordlg->show();
	}
}

/**
 * Current color was set in the selection dialog.
 * If selection was the special value -1, add a new color.
 * @pre dialogsel >= -1 (a valid selection value)
 */
void PaletteWidget::setCurrentColor(const QColor &color)
{
	Q_ASSERT(m_dialogsel > -2);
	if(m_dialogsel == -1) {
		if(m_selection == -1)
			m_selection = m_colorPalette.count();
		m_colorPalette.insertColor(m_selection, color);
	} else {
		m_colorPalette.setColorAt(m_selection, color);
	}
	m_nextColor = color;
	update();
}

/**
 * Dialog was finished, mark dialog selection as unselected
 * so the dialog can be opened again.
 */
void PaletteWidget::dialogDone()
{
	m_dialogsel = -2;
}

/**
 * Handle special events.
 * Currently only tooltip event is handled here. Display a tooltip
 * that describes the color under the pointer.
 */
bool PaletteWidget::event(QEvent *event)
{
	if(event->type() == QEvent::ToolTip) {
		const QPoint pos = (static_cast<const QHelpEvent *>(event))->pos();
		const int index = indexAt(pos);
		if(index >= 0 && index < m_colorPalette.count()) {
			QColor color = m_colorPalette.colorAt(index);
			QToolTip::showText(
				mapToGlobal(pos),
				tr("Red: %1\nGreen: %2\nBlue: %3\nHex: %4")
					.arg(color.red())
					.arg(color.green())
					.arg(color.blue())
					.arg(color.name()),
				this, swatchRect(index));
			event->accept();
			return true;
		}
	}
	return QWidget::event(event);
}

void PaletteWidget::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);

	painter.setPen(Qt::NoPen);
	painter.setBrush(palette().alternateBase());
	painter.drawRect(event->rect());

	int totalCount = m_colorPalette.count() + 1;

	if(m_maxrows > 0)
		totalCount = qMin(m_columns, totalCount);

	for(int i = 0; i < qMin(totalCount, m_colorPalette.count()); ++i) {
		QRect swatch = swatchRect(i);
		painter.fillRect(swatch, m_colorPalette.colorAt(i));
	}

	QRect swatch = swatchRect(m_colorPalette.count()).adjusted(0, 0, -1, -1);
	painter.setBrush(palette().base());
	QPen p(palette().text().color());
	p.setCosmetic(true);
	p.setWidth(painter.device()->devicePixelRatioF());
	painter.setPen(p);
	painter.drawRect(swatch);

	int cx = swatch.left() + swatch.width() / 2;
	int cy = swatch.top() + swatch.height() / 2;
	int d = qMin(swatch.width(), swatch.height()) / 2 - 3;
	painter.drawLine(cx - d, cy, cx + d, cy);
	painter.drawLine(cx, cy - d, cx, cy + d);
}

void PaletteWidget::mousePressEvent(QMouseEvent *event)
{
	m_dragstart = event->pos();
	m_dragTime = QDateTime::currentMSecsSinceEpoch();
	m_selection = indexAt(event->pos());
	if(m_selection == m_colorPalette.count()) {
		// Clicked on the [+] placeholder
		m_selection = -1;
		addColor();

	} else if(m_selection != -1) {
		// Clicked on a valid color
		m_outline->setGeometry(swatchRect(m_selection).adjusted(-1, -1, 1, 1));
		m_outline->show();
		if(event->button() == Qt::LeftButton) {
			emit colorSelected(m_colorPalette.colorAt(m_selection));
		}

	} else {
		m_outline->hide();
	}
}

void PaletteWidget::contextMenuEvent(QContextMenuEvent *event)
{
	m_contextmenu->popup(mapToGlobal(event->pos()));
}

void PaletteWidget::mouseMoveEvent(QMouseEvent *event)
{
	if(m_selection != -1 && (event->buttons() & Qt::LeftButton) &&
	   (event->pos() - m_dragstart).manhattanLength() >
		   QApplication::startDragDistance()) {
		QDrag *drag = new QDrag(this);

		QMimeData *mimedata = new QMimeData;
		const QColor color = m_colorPalette.colorAt(m_selection);
		mimedata->setColorData(color);

		drag->setMimeData(mimedata);
		drag->exec(Qt::CopyAction | Qt::MoveAction);
	}
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent *)
{
	if(m_selection > -1)
		editCurrentColor();
	else
		addColor();
}

void PaletteWidget::keyReleaseEvent(QKeyEvent *event)
{
	if(m_selection != -1 && event->key() == Qt::Key_Delete)
		removeColor();
}

void PaletteWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("application/x-color")) {
		if(event->source() == this)
			event->setDropAction(Qt::MoveAction);
		event->accept();
		m_outline->show();
	}
}

void PaletteWidget::dragMoveEvent(QDragMoveEvent *event)
{
	QPoint pos = compat::dragMovePos(*event);
	const int index = indexAt(pos, true);
	if(index != -1) {
		m_outline->setGeometry(swatchRect(index));
	} else {
		m_outline->setGeometry(
			betweenRect(nearestAt(pos)).adjusted(-1, -1, 1, 1));
	}
}

void PaletteWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
	Q_UNUSED(event);
	if(m_selection != -1 && hasFocus()) {
		m_outline->setGeometry(swatchRect(m_selection).adjusted(-1, -1, 1, 1));
	} else {
		m_outline->hide();
	}
}

void PaletteWidget::focusInEvent(QFocusEvent *)
{
	if(m_selection != -1)
		m_outline->setGeometry(swatchRect(m_selection).adjusted(-1, -1, 1, 1));
}

void PaletteWidget::focusOutEvent(QFocusEvent *)
{
	m_outline->hide();
}

void PaletteWidget::dropEvent(QDropEvent *event)
{
	if(!event->mimeData()->hasFormat("application/x-color")) {
		return;
	}
	event->accept();

	// To avoid accidental swapping of colors, we only react to the drop event
	// if a sufficient amount of time has elapsed.
	if(event->source() == this &&
	   QDateTime::currentMSecsSinceEpoch() - m_dragTime < MIN_DRAG_TIME_MSEC) {
		return;
	}

	QPoint pos = compat::dropPos(*event);
	int index = indexAt(pos, true);
	if(index == m_colorPalette.count()) {
		// Append color
		m_colorPalette.appendColor(
			qvariant_cast<QColor>(event->mimeData()->colorData()));

	} else if(index != -1) {
		if(event->source() == this) {
			// Switch colors
			m_colorPalette.setColorAt(
				m_selection, m_colorPalette.colorAt(index));
		}
		m_colorPalette.setColorAt(
			index, qvariant_cast<QColor>(event->mimeData()->colorData()));
	} else {
		index = nearestAt(pos);
		if(event->source() == this) {
			// Move color
			m_colorPalette.eraseColor(m_selection);
			if(index >= m_selection)
				--index;
		}
		m_colorPalette.insertColor(
			index, qvariant_cast<QColor>(event->mimeData()->colorData()));
	}
	m_selection = index;
	update();
}

void PaletteWidget::wheelEvent(QWheelEvent *event)
{
	if(m_enableScrolling) {
		const QPoint delta = event->angleDelta();
		m_scrollbar->setValue(m_scrollbar->value() - delta.y() / 16);
	}
}

/**
 * @param point coordinates inside the widget
 * @param extrapadding if true, extra room is added between swatches
 * @return index number of color swatch at point
 * @retval -1 if point is outside any color swatch
 */
int PaletteWidget::indexAt(const QPoint &point, bool extraPadding) const
{
	const int normalizedX = point.x() - m_leftMargin - m_spacing;
	const int ix = normalizedX / (m_swatchsize.width() + m_spacing);
	const int nearestX = ix * (m_swatchsize.width() + m_spacing);
	int padding = extraPadding ? 4 : 0;
	int x = normalizedX - nearestX;
	if(x < m_spacing + padding || x > m_swatchsize.width() + m_spacing)
		return -1;

	const int yw = m_spacing + m_swatchsize.height();
	const int y = (point.y() + m_scroll) / yw;
	if(point.y() + m_scroll < y * yw + m_spacing)
		return -1;

	// Allow the extra index if this is an editable palette
	const int index = (y * m_columns) + ix;
	if(index >= m_colorPalette.count()) {
		if(index > m_colorPalette.count())
			return -1;
	}

	return index;
}

/**
 * Return the index nearest to the point. If the position is after
 * the last palette entry, Palette::count() is returned.
 * @param point coordinates inside the widget
 * @return index of the color swatch nearest
 * @pre _palette != 0
 */
int PaletteWidget::nearestAt(const QPoint &point) const
{
	const int x = (point.x() - m_leftMargin - m_spacing) /
				  (m_spacing + m_swatchsize.width());
	const int y = (point.y() + m_scroll) / (m_spacing + m_swatchsize.height());

	return qMin(y * m_columns + x, m_colorPalette.count());
}

/**
 * Get the position of the color swatch
 * @param index of the color swatch
 * @return swatch geometry
 * @pre 0 <= index
 */
QRect PaletteWidget::swatchRect(int index) const
{
	Q_ASSERT(index >= 0);
	return QRect(
		m_leftMargin + m_spacing +
			(m_swatchsize.width() + m_spacing) * (index % m_columns),
		m_spacing + (m_swatchsize.height() + m_spacing) * (index / m_columns) -
			m_scroll,
		m_swatchsize.width(), m_swatchsize.height());
}

/**
 * Get a rectangle between two color swatches.
 * @param index swatch index after the space
 * @return separator area between two swatches
 */
QRect PaletteWidget::betweenRect(int index) const
{
	return QRect(
		m_leftMargin + m_spacing / 2 +
			(m_swatchsize.width() + m_spacing) * (index % m_columns),
		m_spacing + (m_swatchsize.height() + m_spacing) * (index / m_columns) -
			m_scroll,
		m_spacing / 2, m_swatchsize.height());
}

}
