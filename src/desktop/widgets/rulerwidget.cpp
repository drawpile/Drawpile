// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/rulerwidget.h"

#include <QPainter>

#include <array>

namespace widgets {
static constexpr std::array niceDivisions{1,   2,	5,	 10,   20,	 50,
										  100, 200, 500, 1000, 2000, 5000};


RulerWidget::RulerWidget(QWidget *parent)
	: QWidget(parent)
{
	// This widget does not yet support creating guides, so for now don't
	// intercept mouse events to avoid confusion.
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

void RulerWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.fillRect(0, 0, width(), height(), palette().color(QPalette::Base));

	// The ruler doesn't make any sense right now for a rotated canvas, so
	// disable it if the canvas is rotated.
	// TODO: We could project the pixel grid to the ruler and draw ticks at the
	//       intersection.
	if(m_isRotated)
		return;

	// How many canvas pixels are in view.
	qreal distance = m_lMax - m_lMin;
	if(distance < 1)
		return;

	// We want about 60 display independent pixels per division.
	int divisions = longSize() / 60;

	qreal initialPPD = distance / divisions;
	// Having weird numbers for divisions isn't great, so try to fit this to a
	// nearby nice number.
	int adjustedPPD = 1;
	qreal bestDist = niceDivisions.back();
	for(int ppd : niceDivisions) {
		qreal dist = std::abs(initialPPD - ppd);
		if(dist < bestDist) {
			adjustedPPD = ppd;
			bestDist = dist;
		}
	}

	int pixelsPerRulerDivision =
		std::lround((adjustedPPD * m_canvasToRulerScale));
	divisions = longSize() / pixelsPerRulerDivision;

	QColor text = palette().color(QPalette::Text);
	QColor dark =
		QColor(text.red(), text.green(), text.blue(), text.alpha() / 2);

	painter.setPen(dark);
	painter.setRenderHint(QPainter::Antialiasing, true);
	// Set pen for consistent line width on high DPI displays
	QPen pen = painter.pen();
	pen.setWidthF(1.0 / devicePixelRatioF());
	painter.setPen(pen);

	qreal startOffset = std::fmod(std::abs(m_lMin), adjustedPPD);
	int canvasL = std::lround(
		m_lMin < 0 ? (m_lMin + startOffset) - adjustedPPD
				   : m_lMin - startOffset);

	int lineHeight = 0;
	if(!isHorizontal()) {
		// Figure out the largest number we're going to draw so that we can
		// choose a smaller font size if it's too big.
		auto digitsNeeded = [&](int num) {
			int negative = 0;
			if(num < 0) {
				num = -num;
				negative = 1;
			}
			for(int i = 1; i <= std::numeric_limits<int>::digits10; ++i) {
				if(num < std::pow(10, i))
					return i + negative;
			}
			return std::numeric_limits<int>::digits10 + 1 + negative;
		};

		int digits = std::max(
			digitsNeeded(canvasL),
			digitsNeeded(canvasL + adjustedPPD * divisions));

		while(true) {
			lineHeight =
				painter.fontMetrics().tightBoundingRect("0123456789").height();
			lineHeight += 2;

			if(lineHeight * digits >= pixelsPerRulerDivision - 2) {
				QFont smallerFont = painter.font();
				qreal newPointSize = smallerFont.pointSizeF() - 1;
				if(newPointSize <= 0)
					// The font doesn't seem to be responding to point size or
					// something else is wrong. Bail out here and continue with
					// the current font.
					break;
				smallerFont.setPointSizeF(newPointSize);
				painter.setFont(smallerFont);
			} else
				break;
		}
	}

	for(;; canvasL += adjustedPPD) {
		int l = canvasToRuler(canvasL);
		if(l < -pixelsPerRulerDivision)
			continue;
		if(l >= longSize())
			break;
		painter.setPen(dark);
		painter.drawLine(adjustPoint(l, 0), adjustPoint(l, shortSize()));
		painter.setPen(text);
		if(isHorizontal())
			painter.drawText(
				adjustPoint(l + 2, shortSize() - 2), QString::number(canvasL));
		else {
			// Qt can't draw anti-aliased texted when rotated, so draw it
			// vertically. Also, we can't set the line spacing using drawText,
			// so we get to draw each character manually.
			QString label = QString::number(canvasL);
			int offset = lineHeight;
			for(QChar c : label) {
				painter.drawText(2, l + offset, c);
				offset += lineHeight;
			}
		}
	}
}

void RulerWidget::onViewChanged(const QPolygonF &view)
{
	if(isHorizontal()) {
		m_lMin = view.boundingRect().left();
		m_lMax = view.boundingRect().right();
	} else {
		m_lMin = view.boundingRect().top();
		m_lMax = view.boundingRect().bottom();
	}
	update();
}

void RulerWidget::setCanvasToRulerTransform(qreal scale, int offset)
{
	m_canvasToRulerScale = scale;
	m_canvasToRulerOffset = offset;
	update();
}

void RulerWidget::onTransformChanged(qreal zoom [[maybe_unused]], qreal angle)
{
	bool isRotated = std::abs(angle) > 0.001;
	if(isRotated != m_isRotated) {
		m_isRotated = isRotated;
		update();
	}
}
} // namespace widgets
