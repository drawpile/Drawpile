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
	}
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	(void)event;
	QPainter painter(this);

	painter.fillRect(contentsRect(), palette().window());

	int strokew = width() - width()/4;
	int strokeh = height() / 4;
	int offx = width()/8;
	int offy = height()/2;
	double phase = 0;
	double dphase = (2*M_PI)/double(strokew);
	for(int x=0;x<strokew;x++) {
		qreal fx = x/double(strokew);
		qreal pressure = ((fx*fx) - (fx*fx*fx))*6.756;
		QPixmap brush = brush_.getBrush(pressure);
		int r = qRound(brush.width()/2.0);

		int y = qRound(sin(phase) * strokeh);
		phase += dphase;
		painter.drawPixmap(offx+x-r,offy+y-r,brush);
	}

}

/**
 * @param brush brush to set
 */
void BrushPreview::setBrush(const drawingboard::Brush& brush)
{
	brush_ = brush;
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
	update();
}

void BrushPreview::setSizePressure(bool enable)
{
	sizepressure_ = enable;
	if(enable)
		brush_.setRadius2(0);
	else
		brush_.setRadius2(brush_.radius(1.0));
	update();
}

void BrushPreview::setOpacityPressure(bool enable)
{
	opacitypressure_ = enable;
	if(enable)
		brush_.setOpacity2(0);
	else
		brush_.setOpacity2(brush_.opacity(1.0));
	update();
}

void BrushPreview::setHardnessPressure(bool enable)
{
	hardnesspressure_ = enable;
	if(enable)
		brush_.setHardness2(0);
	else
		brush_.setHardness2(brush_.hardness(1.0));
	update();
}

void BrushPreview::setColorPressure(bool enable)
{
	colorpressure_ = enable;
	if(enable)
		brush_.setColor2(palette().color(QPalette::Window));
	else
		brush_.setColor2(palette().color(QPalette::WindowText));
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

