#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QStyleOptionFrameV2>
#include <QMouseEvent>

#include "gradientslider.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

GradientSlider::GradientSlider(QWidget *parent)
	: QAbstractSlider(parent), color1_(Qt::black), color2_(Qt::white),
	saturation_(1), value_(1), mode_(Color), border_(4)
{
}

void GradientSlider::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	// Draw the frame
	QStyleOptionFrameV2 frame;
	frame.initFrom(this);
	frame.midLineWidth = border_;
	frame.state = QStyle::State_Sunken;

	style()->drawPrimitive(QStyle::PE_Frame, &frame, &painter, this);

	// Draw the gradient
	QPointF endpoint;
	if(orientation() == Qt::Horizontal)
		endpoint = QPointF(width()-border_,border_);
	else
		endpoint = QPointF(border_,height()-border_);

	QLinearGradient grad(QPointF(border_,border_),endpoint);

	if(mode_ == Color) {
		grad.setColorAt(0, color1_);
		grad.setColorAt(1, color2_);
	} else {
		for(int i=0;i<=7;++i)
			grad.setColorAt(i/7.0,
					QColor::fromHsvF(i/7.001, saturation_, value_));
	}
	QRect gradrect = contentsRect().adjusted(border_,border_,-border_,-border_);
	painter.setClipRect(gradrect);
	painter.fillRect(gradrect, QBrush(grad));

	// Draw pointer
	qreal pos = (value() - minimum()) / qreal(maximum()-minimum()) *
		(((orientation()==Qt::Horizontal)?width():height())-border_*2);

	static const QPointF hor[6] = {
		QPointF(-0.5,0),
		QPointF(0.5,0),
		QPointF(0,0.5),
		QPointF(-0.5,2),
		QPointF(0.5,2),
		QPointF(0,1.5)
	};
	static const QPointF ver[6] = {
		QPointF(0,-0.5),
		QPointF(0,0.5),
		QPointF(0.5,0),
		QPointF(2,-0.5),
		QPointF(2,0.5),
		QPointF(1.5,0)
	};

	const QPointF *points;
	painter.save();
	qreal scale;
	if(orientation() == Qt::Horizontal) {
		scale = height();
		painter.translate(border_+pos,border_);
		points = hor;
	} else {
		scale = width();
		painter.translate(border_,border_+pos);
		points = ver;
	}
	scale = scale/2 - border_ - 0.5;
	painter.scale(scale,scale);
	painter.setBrush(Qt::black);
	painter.drawPolygon(points,3);
	painter.setBrush(Qt::white);
	painter.drawPolygon(points+3,3);
	painter.restore();

	// Focus rectangle
	if(hasFocus()) {
		painter.setClipRect(contentsRect());
		QStyleOptionFocusRect option;
		option.initFrom(this);
		option.backgroundColor = palette().color(QPalette::Background);

		style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
	}

}

void GradientSlider::mousePressEvent(QMouseEvent *event)
{
	setPosition(event->x(),event->y());
	update();
}

void GradientSlider::mouseMoveEvent(QMouseEvent *event)
{
	setPosition(event->x(),event->y());
	update();
}

void GradientSlider::setPosition(int x,int y)
{
	qreal pos;
	if(orientation() == Qt::Horizontal)
		pos = (x-border_) / qreal(width()-border_*2);
	else
		pos = (y-border_) / qreal(height()-border_*2);
	setValue(qRound(minimum() + pos * (maximum()-minimum())));
}

void GradientSlider::setMode(Mode mode)
{
	mode_ = mode;
	update();
}

void GradientSlider::setColor1(const QColor& color)
{
	color1_ = color;
	update();
}

void GradientSlider::setColor2(const QColor& color)
{
	color2_ = color;
	update();
}

void GradientSlider::setColorSaturation(qreal saturation)
{
	if(saturation<0)
		saturation = 0;
	else if(saturation>1)
		saturation = 1;
	saturation_ = saturation;
	update();
}

void GradientSlider::setColorValue(qreal value)
{
	if(value<0)
		value = 0;
	else if(value>1)
		value = 1;
	value_ = value;
	update();
}

void GradientSlider::setBorderWidth(int width)
{
	if(width<0)
		width = 0;
	border_ = width;
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

