/**
 * \file gradient_slider.cpp
 *
 * \author Mattia Basaglia
 *
 * \copyright Copyright (C) 2013-2017 Mattia Basaglia
 * \copyright Copyright (C) 2014 Calle Laakkonen
 * \copyright Copyright (C) 2017 caryoscelus
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "gradient_slider.hpp"

#include <QPainter>
#include <QStyleOptionSlider>
#include <QLinearGradient>
#include <QMouseEvent>

static void loadResource()
{
    static bool loaded = false;
    if ( !loaded )
    {
        Q_INIT_RESOURCE(color_widgets);
        loaded = true;
    }
}

namespace color_widgets {

class GradientSlider::Private
{
public:
    QLinearGradient gradient;
    QBrush back;

    Private() :
        back(Qt::darkGray, Qt::DiagCrossPattern)
    {
        loadResource();
        back.setTexture(QPixmap(QStringLiteral(":/color_widgets/alphaback.png")));
        gradient.setCoordinateMode(QGradient::StretchToDeviceMode);
        gradient.setSpread(QGradient::RepeatSpread);
    }

	void mouse_event(QMouseEvent *ev, GradientSlider* owner, bool allow_jumps)
	{
		QStyleOptionSlider opt;
		owner->initStyleOption(&opt);
		QRect slider_rect = owner->style()->subControlRect(
			QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, owner
		);

		if ( !allow_jumps && slider_rect.contains(ev->pos()) )
			return;

		QPoint center = slider_rect.center() - slider_rect.topLeft();
		QPoint point = ev->pos() - center;

		QRect groove_rect = owner->style()->subControlRect(
			QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, owner
		);

		int slider_length;
		int slider_min;
		int slider_max;
		int pos;

		if (owner->orientation() == Qt::Horizontal) {
			slider_length = slider_rect.width();
			slider_min = groove_rect.x();
			slider_max = groove_rect.right() - slider_length + 1;
			pos = point.x();
		} else {
			slider_length = slider_rect.height();
			slider_min = groove_rect.y();
			slider_max = groove_rect.bottom() - slider_length + 1;
			pos = point.y();
		}
		owner->setSliderPosition(
			QStyle::sliderValueFromPosition(
				owner->minimum(),
				owner->maximum(),
				pos - slider_min,
				slider_max - slider_min,
				opt.upsideDown
			)
		);
	}

};

GradientSlider::GradientSlider(QWidget *parent) :
    GradientSlider(Qt::Horizontal, parent)
{}

GradientSlider::GradientSlider(Qt::Orientation orientation, QWidget *parent) :
    QSlider(orientation, parent), p(new Private)
{
    setTickPosition(NoTicks);
}

GradientSlider::~GradientSlider()
{
    delete p;
}

void GradientSlider::mousePressEvent(QMouseEvent *ev)
{
	if ( ev->button() == Qt::LeftButton )
	{
		ev->accept();
		setSliderDown(true);
		p->mouse_event(ev, this, false);
		update();
	}
	else
	{
		QSlider::mousePressEvent(ev);
	}
}

void GradientSlider::mouseMoveEvent(QMouseEvent *ev)
{
	if ( ev->buttons() & Qt::LeftButton )
	{
		ev->accept();
		p->mouse_event(ev, this, true);
		update();
	}
	else
	{
		QSlider::mouseMoveEvent(ev);
	}
}

void GradientSlider::mouseReleaseEvent(QMouseEvent *ev)
{
	if ( ev->button() == Qt::LeftButton )
	{
		ev->accept();
		setSliderDown(false);
		update();
	}
	else
	{
		QSlider::mousePressEvent(ev);
	}
}

QBrush GradientSlider::background() const
{
    return p->back;
}

void GradientSlider::setBackground(const QBrush &bg)
{
    p->back = bg;
    update();
}

QGradientStops GradientSlider::colors() const
{
    return p->gradient.stops();
}

void GradientSlider::setColors(const QGradientStops &colors)
{
    p->gradient.setStops(colors);
    update();
}

QLinearGradient GradientSlider::gradient() const
{
    return p->gradient;
}

void GradientSlider::setGradient(const QLinearGradient &gradient)
{
    p->gradient = gradient;
    update();
}

void GradientSlider::setColors(const QVector<QColor> &colors)
{
    QGradientStops stops;
    stops.reserve(colors.size());

    double c = colors.size() - 1;
    if(c==0) {
        stops.append(QGradientStop(0, colors.at(0)));

    } else {
        for(int i=0;i<colors.size();++i) {
            stops.append(QGradientStop(i/c, colors.at(i)));
        }
    }
    setColors(stops);
}

void GradientSlider::setFirstColor(const QColor &c)
{
    QGradientStops stops = p->gradient.stops();
    if(stops.isEmpty())
        stops.push_back(QGradientStop(0.0, c));
    else
        stops.front().second = c;
    p->gradient.setStops(stops);

    update();
}

void GradientSlider::setLastColor(const QColor &c)
{
    QGradientStops stops = p->gradient.stops();
    if(stops.size()<2)
        stops.push_back(QGradientStop(1.0, c));
    else
        stops.back().second = c;
    p->gradient.setStops(stops);
    update();
}

QColor GradientSlider::firstColor() const
{
    QGradientStops s = colors();
    return s.empty() ? QColor() : s.front().second;
}

QColor GradientSlider::lastColor() const
{
    QGradientStops s = colors();
    return s.empty() ? QColor() : s.back().second;
}

void GradientSlider::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QStyleOptionFrame panel;
    panel.initFrom(this);
    panel.lineWidth = 1;
    panel.midLineWidth = 0;
    panel.state |= QStyle::State_Sunken;
    style()->drawPrimitive(QStyle::PE_Frame, &panel, &painter, this);
    QRect r = style()->subElementRect(QStyle::SE_FrameContents, &panel, this);
    painter.setClipRect(r);

    qreal gradient_direction = invertedAppearance() ? -1 : 1;

    if(orientation() == Qt::Horizontal)
        p->gradient.setFinalStop(gradient_direction, 0);
    else
        p->gradient.setFinalStop(0, -gradient_direction);

    painter.setPen(Qt::NoPen);
    painter.setBrush(p->back);
    painter.drawRect(1,1,geometry().width()-2,geometry().height()-2);
    painter.setBrush(p->gradient);
    painter.drawRect(1,1,geometry().width()-2,geometry().height()-2);

    painter.setClipping(false);
    QStyleOptionSlider opt_slider;
    initStyleOption(&opt_slider);
    opt_slider.tickPosition = TicksBothSides;
    opt_slider.state &= ~QStyle::State_HasFocus;
    opt_slider.subControls = QStyle::SC_SliderHandle;
    if (isSliderDown())
    {
        opt_slider.state |= QStyle::State_Sunken;
        opt_slider.activeSubControls = QStyle::SC_SliderHandle;
    }
#if 0 // this breaks slider rendering on macOS.
    opt_slider.rect = style()->subControlRect(QStyle::CC_Slider,&opt_slider,
                                              QStyle::SC_SliderHandle,this);
#endif
    style()->drawComplexControl(QStyle::CC_Slider, &opt_slider, &painter, this);
}

} // namespace color_widgets
