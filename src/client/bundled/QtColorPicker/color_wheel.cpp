/**

@author Mattia Basaglia

@section License

    Copyright (C) 2013-2014 Mattia Basaglia

    This software is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Color Widgets.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "color_wheel.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QLineF>
#include <qmath.h>

enum Mouse_Status
{
    Nothing,
    Drag_Circle,
    Drag_Square
};

class Color_Wheel::Private
{
private:
    Color_Wheel * const w;

public:
    qreal huem, sat, val;
    unsigned int wheel_width;
    Mouse_Status mouse_status;
    QPixmap hue_ring;
    QImage sat_val_square;

    Private(Color_Wheel *widget)
        : w(widget), huem(0), sat(0), val(0),
        wheel_width(20), mouse_status(Nothing)
    { }

    /// Calculate outer wheel radius from idget center
    qreal outer_radius() const
    {
        return qMin(w->geometry().width(), w->geometry().height())/2;
    }

    /// Calculate inner wheel radius from idget center
    qreal inner_radius() const
    {
        return outer_radius()-wheel_width;
    }

    /// Calculate the edge length of the inner square
    qreal square_size() const
    {
        return inner_radius()*qSqrt(2);
    }

    /// return line from center to given point
    QLineF line_to_point(const QPoint &p) const
    {
        return QLineF (w->geometry().width()/2, w->geometry().height()/2, p.x(), p.y());
    }

    /// Updates the inner square that displays the saturation-value selector
    void render_rectangle()
    {
        int sz = square_size();
        sat_val_square = QImage(sz, sz, QImage::Format_RGB32);

        for(int i = 0; i < sz; ++i)
        {
            for(int j = 0;j < sz; ++j)
            {
                sat_val_square.setPixel( i,j,
                        QColor::fromHsvF(huem,double(i)/sz,double(j)/sz).rgb());
            }
        }
    }

    /// Updates the outer ring that displays the hue selector
    void render_ring()
    {
        hue_ring = QPixmap(outer_radius()*2,outer_radius()*2);
        hue_ring.fill(Qt::transparent);
        QPainter painter(&hue_ring);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setCompositionMode(QPainter::CompositionMode_Source);


        const int hue_stops = 24;
        static QConicalGradient gradient_hue(0, 0, 0);
        if ( gradient_hue.stops().size() < hue_stops )
        {
            for ( double a = 0; a < 1.0; a+=1.0/(hue_stops-1) )
            {
                gradient_hue.setColorAt(a,QColor::fromHsvF(a,1,1));
            }
            gradient_hue.setColorAt(1,QColor::fromHsvF(0,1,1));
        }

        painter.translate(outer_radius(),outer_radius());

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(gradient_hue));
        painter.drawEllipse(QPointF(0,0),outer_radius(),outer_radius());

        painter.setBrush(Qt::transparent);//palette().background());
        painter.drawEllipse(QPointF(0,0),inner_radius(),inner_radius());
    }
};

Color_Wheel::Color_Wheel(QWidget *parent) :
    QWidget(parent), p(new Private(this))
{
}

Color_Wheel::~Color_Wheel()
{
    delete p;
}

QColor Color_Wheel::color() const
{
    return QColor::fromHsvF(p->huem, p->sat, p->val);
}

QSize Color_Wheel::sizeHint() const
{
    return QSize(p->wheel_width*5, p->wheel_width*5);
}

qreal Color_Wheel::hue() const
{
    return p->huem;
}

qreal Color_Wheel::saturation() const
{
    return p->sat;
}

qreal Color_Wheel::value() const
{
    return p->val;
}

unsigned int Color_Wheel::wheelWidth() const
{
    return p->wheel_width;
}

void Color_Wheel::setWheelWidth(unsigned int w)
{
    p->wheel_width = w;
    p->render_rectangle();
    update();
}


void Color_Wheel::paintEvent(QPaintEvent * )
{
    double selector_w = 6;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(geometry().width()/2,geometry().height()/2);

    // hue wheel
    if(p->hue_ring.isNull())
        p->render_ring();

    painter.drawPixmap(-p->outer_radius(), -p->outer_radius(), p->hue_ring);

    // hue selector
    painter.setPen(QPen(Qt::black,3));
    painter.setBrush(Qt::NoBrush);
    QLineF ray(0, 0, p->outer_radius(), 0);
    ray.setAngle(p->huem*360);
    QPointF h1 = ray.p2();
    ray.setLength(p->inner_radius());
    QPointF h2 = ray.p2();
    painter.drawLine(h1,h2);

    // lum-sat square
    if(p->sat_val_square.isNull())
        p->render_rectangle();

    painter.rotate(-p->huem*360-45);
    ray.setLength(p->inner_radius());
    ray.setAngle(135);
    painter.drawImage(ray.p2(), p->sat_val_square);

    // lum-sat selector
    //painter.rotate(135);
    painter.setPen(QPen(p->val > 0.5 ? Qt::black : Qt::white, 3));
    painter.setBrush(Qt::NoBrush);
    double max_dist = p->square_size();
    painter.drawEllipse(QPointF(p->sat*max_dist-max_dist/2,
                                p->val*max_dist-max_dist/2
                               ),
                        selector_w, selector_w);

}

void Color_Wheel::mouseMoveEvent(QMouseEvent *ev)
{
    if (p->mouse_status == Drag_Circle )
    {
        p->huem = p->line_to_point(ev->pos()).angle()/360.0;
        p->render_rectangle();

        emit colorSelected(color());
        emit colorChanged(color());
        update();
    }
    else if(p->mouse_status == Drag_Square)
    {
        QLineF glob_mouse_ln = p->line_to_point(ev->pos());
        QLineF center_mouse_ln ( QPointF(0,0),
                                 glob_mouse_ln.p2() - glob_mouse_ln.p1() );
        center_mouse_ln.setAngle(center_mouse_ln.angle()-p->huem*360-45);

        p->sat = qBound(0.0, center_mouse_ln.x2()/p->square_size()+0.5, 1.0);

        p->val = qBound(0.0, center_mouse_ln.y2()/p->square_size()+0.5, 1.0);

        emit colorSelected(color());
        emit colorChanged(color());
        update();
    }
}

void Color_Wheel::mousePressEvent(QMouseEvent *ev)
{
    if ( ev->buttons() & Qt::LeftButton )
    {
        QLineF ray = p->line_to_point(ev->pos());
        if ( ray.length() <= p->inner_radius() )
            p->mouse_status = Drag_Square;
        else if ( ray.length() <= p->outer_radius() )
            p->mouse_status = Drag_Circle;
    }
}

void Color_Wheel::mouseReleaseEvent(QMouseEvent *ev)
{
    mouseMoveEvent(ev);
    p->mouse_status = Nothing;
}

void Color_Wheel::resizeEvent(QResizeEvent *)
{
    p->render_ring();
    p->render_rectangle();
}

void Color_Wheel::setColor(QColor c)
{
    qreal oldh = p->huem;
    p->huem = qMax(0.0, c.hueF());
    p->sat = c.saturationF();
    p->val = c.valueF();
    if (!qFuzzyCompare(oldh+1, p->huem+1))
        p->render_rectangle();
    update();
    emit colorChanged(c);
}

void Color_Wheel::setHue(qreal h)
{
    p->huem = qBound(0.0, h, 1.0);
    p->render_rectangle();
    update();
}

void Color_Wheel::setSaturation(qreal s)
{
    p->sat = qBound(0.0, s, 1.0);
    update();
}

void Color_Wheel::setValue(qreal v)
{
    p->val = qBound(0.0, v, 1.0);
    update();
}

