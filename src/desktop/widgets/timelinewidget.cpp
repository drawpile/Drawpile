/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 Calle Laakkonen

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

#include "timelinewidget.h"
#include "canvas/timelinemodel.h"
#include "net/envelopebuilder.h"
#include "utils/qtguicompat.h"

#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QHash>
#include <QScrollBar>

namespace {
static const int MAX_LAYERS_PER_FRAME = sizeof(rustpile::Frame) / sizeof(rustpile::LayerID);
}

namespace widgets {

struct TimelineWidget::Private {
	canvas::TimelineModel *model = nullptr;

	int headerWidth=10;
	int columnWidth=10;
	int rowHeight=10;
	int xScroll=0;
	int yScroll=0;
	int currentFrame=1;
	int currentLayerId=0;
	bool editable=true;

	QScrollBar *verticalScroll;
	QScrollBar *horizontalScroll;
};

TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget{parent}, d{new Private}
{
	d->verticalScroll = new QScrollBar(Qt::Vertical, this);
	d->horizontalScroll = new QScrollBar(Qt::Horizontal, this);

	connect(d->horizontalScroll, &QScrollBar::valueChanged, this, &TimelineWidget::setHorizontalScroll);
	connect(d->verticalScroll, &QScrollBar::valueChanged, this, &TimelineWidget::setVerticalScroll);
}

TimelineWidget::~TimelineWidget()
{
	delete d;
}


void TimelineWidget::setModel(canvas::TimelineModel *model)
{
	d->model = model;
	connect(model, &canvas::TimelineModel::framesChanged, this, QOverload<>::of(&TimelineWidget::update), Qt::QueuedConnection);
	connect(model, &canvas::TimelineModel::layersChanged, this, &TimelineWidget::onLayersChanged, Qt::QueuedConnection);
}

void TimelineWidget::setEditable(bool editable)
{
	d->editable = editable;
	update();
}

canvas::TimelineModel *TimelineWidget::model() const {
	return d->model;
}

void TimelineWidget::setCurrentFrame(int frame)
{
	if(frame != d->currentFrame) {
		d->currentFrame = frame;
		update();
	}
}

void TimelineWidget::setCurrentLayer(int layerId)
{
	if(layerId != d->currentLayerId) {
		d->currentLayerId = layerId;
		update();
	}
}

int TimelineWidget::currentFrame() const
{
	return d->currentFrame;
}

int TimelineWidget::currentLayerId() const
{
	return d->currentLayerId;
}

void TimelineWidget::onLayersChanged()
{
	Q_ASSERT(d->model);
	const auto fm = this->fontMetrics();
	d->headerWidth = 10;
	d->rowHeight = fm.height() * 3 / 2;
	d->columnWidth = fm.horizontalAdvance('9') * 3;

	for(const auto &l : d->model->layers()) {
		const int w = fm.horizontalAdvance(l.name) + 10;
		if(w>d->headerWidth)
			d->headerWidth = w;
	}

	updateScrollbars();
	update();
}

void TimelineWidget::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event);

	const QSize hsh = d->horizontalScroll->sizeHint();
	const QSize vsh = d->verticalScroll->sizeHint();

	d->horizontalScroll->setGeometry(
				d->headerWidth, height() - hsh.height(),
				width() - d->headerWidth, hsh.height()
				);
	d->verticalScroll->setGeometry(
				width()-vsh.width(), 0,
				vsh.width(), height() - hsh.height()
				);
	d->horizontalScroll->setPageStep(width());
	d->verticalScroll->setPageStep(height());

	updateScrollbars();
}

void TimelineWidget::updateScrollbars()
{
	if(!d->model)
		return;

	const QSize hsh = d->horizontalScroll->sizeHint();
	const QSize vsh = d->verticalScroll->sizeHint();

	d->horizontalScroll->setMaximum(
	            qMax(0, (d->headerWidth + vsh.width() + (d->model->frames().size()+1) * d->columnWidth) - width()));
	d->verticalScroll->setMaximum(
	            qMax(0, (hsh.height() + d->model->layers().size() * d->rowHeight) - height()));

}
void TimelineWidget::setHorizontalScroll(int pos)
{
	d->xScroll = pos;
	update();
}

void TimelineWidget::setVerticalScroll(int pos)
{
	d->yScroll = pos;
	update();
}

void TimelineWidget::paintEvent(QPaintEvent *)
{
	if(!d->model)
		return;

	const int w = width();
	const int h = height();

	const int vLine = qMin(h, d->model->layers().size() * d->rowHeight - d->yScroll);
	const int hLine = qMin(w, d->headerWidth + d->model->frames().size() * d->columnWidth - d->xScroll);

	QPainter painter(this);

	const QPalette &pal = this->palette();
	const QColor selectedColor = d->model->isManualMode() && d->editable ? pal.windowText().color() : pal.color(QPalette::Disabled, QPalette::WindowText);
	const QColor gridColor = pal.color(QPalette::Button);
	const QColor highlightColor = pal.highlight().color();

	const int selectedRow = d->model->layerRow(d->currentLayerId);

	// Selected layer background
	QColor layerHighlightColor = highlightColor;
	layerHighlightColor.setAlphaF(0.25);
	painter.fillRect(
		0,
		-d->yScroll + selectedRow * d->rowHeight,
		d->headerWidth + d->model->frames().size() * d->columnWidth,
		d->rowHeight,
		layerHighlightColor
	);

	// Frame columns
	int x = d->headerWidth - d->xScroll;
	painter.setClipRect(d->headerWidth, 0, w, h);

	painter.setPen(gridColor);

	int frameNum = 1;
	for(const auto &col : d->model->frames()) {
		if(x+d->columnWidth > d->headerWidth) {
			if(x > w)
				break;

			if(frameNum == d->currentFrame) {
				painter.fillRect(x, 0, d->columnWidth, vLine, highlightColor);
			}

			for(int i=0;i<MAX_LAYERS_PER_FRAME;++i) {
				if(col.frame[i] == 0)
					break;
				const int y = d->model->layerRow(col.frame[i]) * d->rowHeight - d->yScroll;
				painter.fillRect(x, y, d->columnWidth, d->rowHeight, selectedColor);
			}

			painter.drawLine(x, 0, x, vLine);
		}
		x += d->columnWidth;
		++frameNum;
	}
	painter.drawLine(x, 0, x, vLine);

	painter.setClipping(false);

	// Layer row labels
	int y = -d->yScroll;
	for(const auto &row : d->model->layers()) {
		painter.setPen(pal.windowText().color());
		painter.drawText(0, y, d->headerWidth - 5, d->rowHeight, Qt::AlignVCenter | Qt::AlignRight, row.name);
		if(x < w)
			painter.drawText(x, y, d->columnWidth, d->rowHeight, Qt::AlignVCenter | Qt::AlignCenter, QStringLiteral("+"));

		y += d->rowHeight;

		painter.setPen(gridColor);
		painter.drawLine(0, y, hLine, y);
	}
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
	const auto pd = event->pixelDelta();
	d->verticalScroll->setValue(d->verticalScroll->value() - pd.y());
	d->horizontalScroll->setValue(d->horizontalScroll->value() - pd.x());
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
	if(!d->model)
		return;

	const auto mousePos = compat::mousePos(*event);

	const int row = (mousePos.y()+d->yScroll) / d->rowHeight;
	if(row >= d->model->layers().size() || mousePos.x() < d->headerWidth)
		return;

	const int col = qMin(
		(mousePos.x() - d->headerWidth + d->xScroll) / d->columnWidth,
	    d->model->frames().size()
	);

	if(event->button() == Qt::LeftButton) {
		if(d->model->isManualMode() && d->editable) {
			net::EnvelopeBuilder eb;
			d->model->makeToggleCommand(eb, col, row);
			emit timelineEditCommand(eb.toEnvelope());
		}
	} else if(event->button() == Qt::MiddleButton) {
		emit selectFrameRequest(
			qMin(col+1, d->model->frames().size()),
			d->model->layerRowId(row)
		);
	}
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	if(!d->model || !d->model->isManualMode() || event->button() != Qt::RightButton || !d->editable)
		return;

	const auto mousePos = compat::mousePos(*event);
	const int col = qMin(
	    (mousePos.x() - d->headerWidth + d->xScroll) / d->columnWidth,
	    d->model->frames().size() - 1
	);

	net::EnvelopeBuilder eb;
	eb.buildUndoPoint(0);
	d->model->makeRemoveCommand(eb, col);
	emit timelineEditCommand(eb.toEnvelope());
}

}
