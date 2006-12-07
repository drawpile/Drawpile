#include <QPainter>
#include <QEvent>
#include <cmath>

#include "brushpreview.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushPreview::BrushPreview(QWidget *parent)
	: QWidget(parent)
{
	setMinimumSize(32,32);
	brush_.setColor(palette().color(QPalette::WindowText));
	brush_.setColor2(palette().color(QPalette::WindowText));
}

void BrushPreview::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);
	if(event->type() == QEvent::PaletteChange) {
		brush_.setColor(palette().color(QPalette::WindowText));
		if(colorpressure_)
			brush_.setColor2(palette().color(QPalette::Window));
		else
			brush_.setColor2(palette().color(QPalette::WindowText));
		updatePreview();
	}
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	updatePreview();
}

void BrushPreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.drawImage(QPoint(0,0), preview_);
}

void BrushPreview::updatePreview()
{
	if(preview_.size() != size())
		preview_ = QImage(size(), QImage::Format_RGB32);
	preview_.fill(palette().color(QPalette::Window).rgb());

	const int strokew = width() - width()/4;
	const int strokeh = height() / 4;
	const int offx = width()/8;
	const int offy = height()/2;
	const double dphase = (2*M_PI)/double(strokew);
	double phase = 0;
	for(int x=0;x<strokew;x++, phase += dphase) {
		const qreal fx = x/qreal(strokew);
		qreal pressure = ((fx*fx) - (fx*fx*fx))*6.756;
		if(pressure<0)
			pressure = 0;
		else if(pressure>1)
			pressure = 1;
		const int rad = brush_.radius(pressure);
		const int y = qRound(sin(phase) * strokeh);
		brush_.draw(preview_,QPoint(offx+x-rad,offy+y-rad),pressure);
	}
}

/**
 * @param brush brush to set
 */
void BrushPreview::setBrush(const drawingboard::Brush& brush)
{
	brush_ = brush;
	updatePreview();
	update();
}

/**
 * @param size brush size
 */
void BrushPreview::setSize(int size)
{
	brush_.setRadius(size);
	if(sizepressure_==false)
		brush_.setRadius2(size);
	updatePreview();
	update();
}

/**
 * @param opacity brush opacity. Range is [0..100]
 */
void BrushPreview::setOpacity(int opacity)
{
	qreal o = opacity/100.0;
	brush_.setOpacity(o);
	if(opacitypressure_==false)
		brush_.setOpacity2(o);
	updatePreview();
	update();
}

/**
 * @param hardness brush hardness . Range is [0..100]
 */
void BrushPreview::setHardness(int hardness)
{
	qreal h = hardness/100.0;
	brush_.setHardness(h);
	if(hardnesspressure_==false)
		brush_.setHardness2(h);
	updatePreview();
	update();
}

void BrushPreview::setSizePressure(bool enable)
{
	sizepressure_ = enable;
	if(enable)
		brush_.setRadius2(0);
	else
		brush_.setRadius2(brush_.radius(1.0));
	updatePreview();
	update();
}

void BrushPreview::setOpacityPressure(bool enable)
{
	opacitypressure_ = enable;
	if(enable)
		brush_.setOpacity2(0);
	else
		brush_.setOpacity2(brush_.opacity(1.0));
	updatePreview();
	update();
}

void BrushPreview::setHardnessPressure(bool enable)
{
	hardnesspressure_ = enable;
	if(enable)
		brush_.setHardness2(0);
	else
		brush_.setHardness2(brush_.hardness(1.0));
	updatePreview();
	update();
}

void BrushPreview::setColorPressure(bool enable)
{
	colorpressure_ = enable;
	if(enable)
		brush_.setColor2(palette().color(QPalette::Window));
	else
		brush_.setColor2(palette().color(QPalette::WindowText));
	updatePreview();
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

