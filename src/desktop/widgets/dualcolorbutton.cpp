// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/dualcolorbutton.h"
#include "desktop/utils/qtguicompat.h"
#include <QHelpEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QStyle>

namespace widgets {

DualColorButton::DualColorButton(QWidget *parent)
	: QWidget(parent)
{
	setMouseTracking(true);
}

QSize DualColorButton::sizeHint() const
{
	return minimumSizeHint();
}

QSize DualColorButton::minimumSizeHint() const
{
	int size = style()->pixelMetric(QStyle::PM_LargeIconSize);
	return QSize(size, size);
}

QString DualColorButton::foregroundText()
{
	return tr("Set foreground color…");
}

QString DualColorButton::backgroundText()
{
	return tr("Set background color…");
}

QString DualColorButton::resetText()
{
	return tr("Reset colors to black and white");
}

QString DualColorButton::swapText()
{
	return tr("Swap foreground and background color");
}

void DualColorButton::setForegroundColor(const QColor &foregroundColor)
{
	if(foregroundColor.isValid() && foregroundColor != m_foregroundColor) {
		m_foregroundColor = foregroundColor;
		update();
	}
}

void DualColorButton::setBackgroundColor(const QColor &backgroundColor)
{
	if(backgroundColor.isValid() && backgroundColor != m_backgroundColor) {
		m_backgroundColor = backgroundColor;
		update();
	}
}

bool DualColorButton::event(QEvent *event)
{
	if(event->type() == QEvent::ToolTip) {
		QPointF pos = QPointF(static_cast<QHelpEvent *>(event)->pos());
		setToolTip(getToolTipForAction(getActionAt(pos)));
	}
	return QWidget::event(event);
}

void DualColorButton::mousePressEvent(QMouseEvent *event)
{
	switch(getActionAt(compat::mousePos(*event))) {
	case Action::Foreground:
		emit foregroundClicked();
		break;
	case Action::Background:
		emit backgroundClicked();
		break;
	case Action::Reset:
		emit resetClicked();
		break;
	case Action::Swap:
		emit swapClicked();
		break;
	case Action::None:
		break;
	}
}

void DualColorButton::mouseMoveEvent(QMouseEvent *event)
{
	updateHoveredAction(getActionAt(compat::mousePos(*event)));
}

void DualColorButton::leaveEvent(QEvent *event)
{
	Q_UNUSED(event);
	updateHoveredAction(Action::None);
}

void DualColorButton::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QSizeF sf(size());
	qreal w = sf.width();
	qreal h = sf.height();
	QMarginsF margins(1.0, 1.0, 1.0, 1.0);
	QRectF colorRect(0.0, 0.0, w * COLOR_RECT_RATIO, h * COLOR_RECT_RATIO);
	QRectF resetRect(0.0, 0.0, w * RESET_RECT_RATIO, h * RESET_RECT_RATIO);

	QPainter painter(this);
	const QPalette &pal = palette();
	const QBrush &hoverBrush = pal.brush(QPalette::ButtonText);
	const QBrush &normalBrush =
		pal.brush(QPalette::Disabled, QPalette::ButtonText);

	const QBrush &backgroundBrush =
		m_hoveredAction == Action::Background ? hoverBrush : normalBrush;
	painter.setPen(QPen(backgroundBrush, 1));
	painter.setBrush(m_backgroundColor);
	painter.drawRect(
		colorRect.translated(w - colorRect.width(), h - colorRect.height())
			.marginsRemoved(margins));

	const QBrush &foregroundBrush =
		m_hoveredAction == Action::Foreground ? hoverBrush : normalBrush;
	painter.setPen(QPen(foregroundBrush, 1));
	painter.setBrush(m_foregroundColor);
	painter.drawRect(colorRect.translated(-1.0, -1.0).marginsRemoved(margins));

	const QBrush &resetBrush =
		m_hoveredAction == Action::Reset ? hoverBrush : normalBrush;
	painter.setPen(QPen(resetBrush, 1));
	painter.setBrush(Qt::white);
	painter.drawRect(resetRect.translated(w * (5.0 / 32.0), h * (23.0 / 32.0))
						 .marginsRemoved(margins));
	painter.setBrush(Qt::black);
	painter.drawRect(
		resetRect.translated(0.0, h * (18.0 / 32.0)).marginsRemoved(margins));

	const QBrush &swapBrush =
		m_hoveredAction == Action::Swap ? hoverBrush : normalBrush;
	painter.setPen(QPen(swapBrush, ((w + h) / 2.0) * (3.0 / 32.0)));
	painter.drawPolyline(QPolygonF(QVector<QPointF>{
		{w * (23.0 / 32.0), h * (4.0 / 32.0)},
		{w * (27.0 / 32.0), h * (4.0 / 32.0)},
		{w * (27.0 / 32.0), h * (8.0 / 32.0)},
	}));
	painter.setPen(
		QPen(swapBrush, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
	painter.setBrush(swapBrush);
	painter.drawPolygon(QPolygonF(QVector<QPointF>{
		{w * (19.0 / 32.0), h * (4.0 / 32.0)},
		{w * (22.0 / 32.0), h * (1.0 / 32.0)},
		{w * (22.0 / 32.0), h * (7.1 / 32.0)},
	}));
	painter.drawPolygon(QPolygonF(QVector<QPointF>{
		{w * (27.0 / 32.0), h * (12.0 / 32.0)},
		{w * (24.0 / 32.0), h * (9.0 / 32.0)},
		{w * (30.0 / 32.0), h * (9.0 / 32.0)},
	}));
}

void DualColorButton::updateHoveredAction(Action hoveredAction)
{
	if(m_hoveredAction != hoveredAction) {
		m_hoveredAction = hoveredAction;
		update();
	}
}

QString DualColorButton::getToolTipForAction(Action action)
{
	switch(action) {
	case Action::Foreground:
		return foregroundText();
	case Action::Background:
		return backgroundText();
	case Action::Reset:
		return resetText();
	case Action::Swap:
		return swapText();
	case Action::None:
		break;
	}
	return QString();
}

DualColorButton::Action DualColorButton::getActionAt(const QPointF &pos)
{
	qreal x = pos.x();
	qreal y = pos.y();
	QSizeF sf(size());
	qreal w = sf.width();
	qreal h = sf.height();

	QRectF colorRect(0.0, 0.0, w * COLOR_RECT_RATIO, h * COLOR_RECT_RATIO);
	if(x <= colorRect.right() && y <= colorRect.bottom()) {
		return Action::Foreground;
	}

	colorRect.translate(w - colorRect.width(), h - colorRect.height());
	if(x >= colorRect.left() && y >= colorRect.top()) {
		return Action::Background;
	}

	if(x < colorRect.left()) {
		return Action::Reset;
	}

	if(y < colorRect.top()) {
		return Action::Swap;
	}

	return Action::None;
}

}
