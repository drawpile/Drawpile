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
#include <cmath>

enum Mouse_Status
{
    Nothing,
    Drag_Circle,
    Drag_Square
};

static const Color_Wheel::Display_Flags hard_default_flags = Color_Wheel::SHAPE_TRIANGLE|Color_Wheel::ANGLE_ROTATING|Color_Wheel::COLOR_HSV;
static Color_Wheel::Display_Flags default_flags = hard_default_flags;
static const double selector_radius = 6;

static qreal color_chromaF(const QColor& c)
{
    qreal max = qMax(c.redF(), qMax(c.greenF(), c.blueF()));
    qreal min = qMin(c.redF(), qMin(c.greenF(), c.blueF()));
    return max - min;
}
static qreal color_lumaF(const QColor& c)
{
    return 0.30 * c.redF() + 0.59 * c.greenF() + 0.11 * c.blueF();
}
static QColor color_from_lch(qreal hue, qreal chroma, qreal luma, qreal alpha = 1 )
{
    qreal h1 = hue*6;
    qreal x = chroma*(1-qAbs(std::fmod(h1,2)-1));
    QColor col;
    if ( h1 >= 0 && h1 < 1 )
        col = QColor::fromRgbF(chroma,x,0);
    else if ( h1 < 2 )
        col = QColor::fromRgbF(x,chroma,0);
    else if ( h1 < 3 )
        col = QColor::fromRgbF(0,chroma,x);
    else if ( h1 < 4 )
        col = QColor::fromRgbF(0,x,chroma);
    else if ( h1 < 5 )
        col = QColor::fromRgbF(x,0,chroma);
    else if ( h1 < 6 )
        col = QColor::fromRgbF(chroma,0,x);

    qreal m = luma - color_lumaF(col);

    return QColor::fromRgbF(
        qBound(0.0,col.redF()+m,1.0),
        qBound(0.0,col.greenF()+m,1.0),
        qBound(0.0,col.blueF()+m,1.0),
        alpha);
}
static QColor rainbow_lch(qreal hue)
{
    return color_from_lch(hue,1,1);
}
static QColor rainbow_hsv(qreal hue)
{
    return QColor::fromHsvF(hue,1,1);
}

static qreal color_lightnessF(const QColor& c)
{
    return ( qMax(c.redF(),qMax(c.greenF(),c.blueF())) +
             qMin(c.redF(),qMin(c.greenF(),c.blueF())) ) / 2;
}
static qreal color_HSL_saturationF(const QColor& col)
{
    qreal c = color_chromaF(col);
    qreal l = color_lightnessF(col);
    if ( qFuzzyCompare(l+1,1) || qFuzzyCompare(l+1,2) )
        return 0;
    return c / (1-qAbs(2*l-1));
}
static QColor color_from_hsl(qreal hue, qreal sat, qreal lig, qreal alpha = 1 )
{
    qreal chroma = (1 - qAbs(2*lig-1))*sat;
    qreal h1 = hue*6;
    qreal x = chroma*(1-qAbs(std::fmod(h1,2)-1));
    QColor col;
    if ( h1 >= 0 && h1 < 1 )
        col = QColor::fromRgbF(chroma,x,0);
    else if ( h1 < 2 )
        col = QColor::fromRgbF(x,chroma,0);
    else if ( h1 < 3 )
        col = QColor::fromRgbF(0,chroma,x);
    else if ( h1 < 4 )
        col = QColor::fromRgbF(0,x,chroma);
    else if ( h1 < 5 )
        col = QColor::fromRgbF(x,0,chroma);
    else if ( h1 < 6 )
        col = QColor::fromRgbF(chroma,0,x);

    qreal m = lig-chroma/2;

    return QColor::fromRgbF(
        qBound(0.0,col.redF()+m,1.0),
        qBound(0.0,col.greenF()+m,1.0),
        qBound(0.0,col.blueF()+m,1.0),
        alpha);
}

class Color_Wheel::Private
{
private:
    Color_Wheel * const w;

public:

    qreal hue, sat, val;
    unsigned int wheel_width;
    Mouse_Status mouse_status;
    QPixmap hue_ring;
    QImage inner_selector;
    Display_Flags display_flags;
    QColor (*color_from)(qreal,qreal,qreal,qreal);
    QColor (*rainbow_from_hue)(qreal);

    Private(Color_Wheel *widget)
        : w(widget), hue(0), sat(0), val(0),
        wheel_width(20), mouse_status(Nothing),
        display_flags(FLAGS_DEFAULT),
        color_from(&QColor::fromHsvF), rainbow_from_hue(&rainbow_hsv)
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

    /// Calculate the height of the inner triangle
    qreal triangle_height() const
    {
        return inner_radius()*3/2;
    }

    /// Calculate the side of the inner triangle
    qreal triangle_side() const
    {
        return inner_radius()*qSqrt(3);
    }

    /// return line from center to given point
    QLineF line_to_point(const QPoint &p) const
    {
        return QLineF (w->geometry().width()/2, w->geometry().height()/2, p.x(), p.y());
    }

    void render_square()
    {
        int sz = square_size();
        inner_selector = QImage(sz, sz, QImage::Format_RGB32);


        for(int i = 0; i < sz; ++i)
        {
            for(int j = 0;j < sz; ++j)
            {
                inner_selector.setPixel( i, j,
                        color_from(hue,double(i)/sz,double(j)/sz,1).rgb());
            }
        }
    }

    /**
     * \brief renders the selector as a triangle
     * \note It's the same as a square with the edge with value=0 collapsed to a single point
     */
    void render_triangle()
    {
        qreal side = triangle_side();
        qreal height = triangle_height();
        qreal ycenter = side/2;
        inner_selector = QImage(height, side, QImage::Format_RGB32);

        for (int x = 0; x < inner_selector.width(); x++ )
        {
            qreal pval = x / height;
            qreal slice_h = side * pval;
            for (int y = 0; y < inner_selector.height(); y++ )
            {
                qreal ymin = ycenter-slice_h/2;
                qreal psat = qBound(0.0,(y-ymin)/slice_h,1.0);

                inner_selector.setPixel(x,y,color_from(hue,psat,pval,1).rgb());
            }
        }
    }

    /// Updates the inner image that displays the saturation-value selector
    void render_inner_selector()
    {
        if ( display_flags & Color_Wheel::SHAPE_TRIANGLE )
            render_triangle();
        else
            render_square();
    }

    /// Offset of the selector image
    QPointF selector_image_offset()
    {
        if ( display_flags & SHAPE_TRIANGLE )
                return QPointF(-inner_radius(),-triangle_side()/2);
        return QPointF(-square_size()/2,-square_size()/2);
    }


    /// Rotation of the selector image
    qreal selector_image_angle()
    {
        if ( display_flags & SHAPE_TRIANGLE )
        {
            if ( display_flags & ANGLE_ROTATING )
                return -hue*360-60;
            return -150;
        }
        else
        {
            if ( display_flags & ANGLE_ROTATING )
                return -hue*360-45;
            else
                return 180;
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
        QConicalGradient gradient_hue(0, 0, 0);
        if ( gradient_hue.stops().size() < hue_stops )
        {
            for ( double a = 0; a < 1.0; a+=1.0/(hue_stops-1) )
            {
                gradient_hue.setColorAt(a,rainbow_from_hue(a));
            }
            gradient_hue.setColorAt(1,rainbow_from_hue(0));
        }

        painter.translate(outer_radius(),outer_radius());

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(gradient_hue));
        painter.drawEllipse(QPointF(0,0),outer_radius(),outer_radius());

        painter.setBrush(Qt::transparent);//palette().background());
        painter.drawEllipse(QPointF(0,0),inner_radius(),inner_radius());
    }

    void set_color(const QColor& c)
    {
        if ( display_flags & Color_Wheel::COLOR_HSV )
        {
            hue = qMax(0.0, c.hsvHueF());
            sat = c.hsvSaturationF();
            val = c.valueF();
        }
        else if ( display_flags & Color_Wheel::COLOR_HSL )
        {
            hue = qMax(0.0, c.hueF());
            sat = color_HSL_saturationF(c);
            val = color_lightnessF(c);
        }
        else if ( display_flags & Color_Wheel::COLOR_LCH )
        {
            hue = qMax(0.0, c.hsvHueF());
            sat = color_chromaF(c);
            val = color_lumaF(c);
        }
    }
};

Color_Wheel::Color_Wheel(QWidget *parent) :
    QWidget(parent), p(new Private(this))
{
    setDisplayFlags(FLAGS_DEFAULT);
}

Color_Wheel::~Color_Wheel()
{
    delete p;
}

QColor Color_Wheel::color() const
{
    return p->color_from(p->hue, p->sat, p->val, 1);
}

QSize Color_Wheel::sizeHint() const
{
    return QSize(p->wheel_width*5, p->wheel_width*5);
}

qreal Color_Wheel::hue() const
{
    if ( (p->display_flags & COLOR_LCH) && p->sat > 0.01 )
        return color().hueF();
    return p->hue;
}

qreal Color_Wheel::saturation() const
{
    return color().hsvSaturationF();
}

qreal Color_Wheel::value() const
{
    return color().valueF();
}

unsigned int Color_Wheel::wheelWidth() const
{
    return p->wheel_width;
}

void Color_Wheel::setWheelWidth(unsigned int w)
{
    p->wheel_width = w;
    p->render_inner_selector();
    update();
}

void Color_Wheel::paintEvent(QPaintEvent * )
{
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
    ray.setAngle(p->hue*360);
    QPointF h1 = ray.p2();
    ray.setLength(p->inner_radius());
    QPointF h2 = ray.p2();
    painter.drawLine(h1,h2);

    // lum-sat square
    if(p->inner_selector.isNull())
        p->render_inner_selector();

    painter.rotate(p->selector_image_angle());
    painter.translate(p->selector_image_offset());

    QPointF selector_position;
    if ( p->display_flags & SHAPE_SQUARE )
    {
        qreal side = p->square_size();
        selector_position = QPointF(p->sat*side, p->val*side);
    }
    else if ( p->display_flags & SHAPE_TRIANGLE )
    {
        qreal side = p->triangle_side();
        qreal height = p->triangle_height();
        qreal slice_h = side * p->val;
        qreal ymin = side/2-slice_h/2;

        selector_position = QPointF(p->val*height, ymin + p->sat*slice_h);
        QPolygonF triangle;
        triangle.append(QPointF(0,side/2));
        triangle.append(QPointF(height,0));
        triangle.append(QPointF(height,side));
        QPainterPath clip;
        clip.addPolygon(triangle);
        painter.setClipPath(clip);
    }

    painter.drawImage(0,0,p->inner_selector);
    painter.setClipping(false);

    // lum-sat selector
    painter.setPen(QPen(p->val > 0.5 ? Qt::black : Qt::white, 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(selector_position, selector_radius, selector_radius);

}

void Color_Wheel::mouseMoveEvent(QMouseEvent *ev)
{
    if (p->mouse_status == Drag_Circle )
    {
        p->hue = p->line_to_point(ev->pos()).angle()/360.0;
        p->render_inner_selector();

        emit colorSelected(color());
        emit colorChanged(color());
        update();
    }
    else if(p->mouse_status == Drag_Square)
    {
        QLineF glob_mouse_ln = p->line_to_point(ev->pos());
        QLineF center_mouse_ln ( QPointF(0,0),
                                 glob_mouse_ln.p2() - glob_mouse_ln.p1() );

        center_mouse_ln.setAngle(center_mouse_ln.angle()+p->selector_image_angle());
        center_mouse_ln.setP2(center_mouse_ln.p2()-p->selector_image_offset());

        if ( p->display_flags & SHAPE_SQUARE )
        {
            p->sat = qBound(0.0, center_mouse_ln.x2()/p->square_size(), 1.0);
            p->val = qBound(0.0, center_mouse_ln.y2()/p->square_size(), 1.0);
        }
        else if ( p->display_flags & SHAPE_TRIANGLE )
        {
            QPointF pt = center_mouse_ln.p2();

            qreal side = p->triangle_side();
            p->val = qBound(0.0, pt.x() / p->triangle_height(), 1.0);
            qreal slice_h = side * p->val;

            qreal ycenter = side/2;
            qreal ymin = ycenter-slice_h/2;
            qreal ymax = ycenter+slice_h/2;

            if ( pt.y() >= ymin && pt.y() <= ymax )
                p->sat = (pt.y()-ymin)/slice_h;
        }

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
        /// \todo if click inside with distance from the selector indicator
        /// > selector_radius => place it there directly
        /// (without the need to drag)
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
    p->render_inner_selector();
}

void Color_Wheel::setColor(QColor c)
{
    qreal oldh = p->hue;
    p->set_color(c);
    if (!qFuzzyCompare(oldh+1, p->hue+1))
        p->render_inner_selector();
    update();
    emit colorChanged(c);
}

void Color_Wheel::setHue(qreal h)
{
    p->hue = qBound(0.0, h, 1.0);
    p->render_inner_selector();
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


void Color_Wheel::setDisplayFlags(Display_Flags flags)
{
    if ( ! (flags & COLOR_FLAGS) )
        flags |= default_flags & COLOR_FLAGS;
    if ( ! (flags & ANGLE_FLAGS) )
        flags |= default_flags & ANGLE_FLAGS;
    if ( ! (flags & SHAPE_FLAGS) )
        flags |= default_flags & SHAPE_FLAGS;

    if ( (flags & COLOR_FLAGS) != (p->display_flags & COLOR_FLAGS) )
    {
        QColor old_col = color();
        if ( flags & Color_Wheel::COLOR_HSL )
        {
            p->hue = old_col.hueF();
            p->sat = color_HSL_saturationF(old_col);
            p->val = color_lightnessF(old_col);
            p->color_from = &color_from_hsl;
            p->rainbow_from_hue = &rainbow_hsv;
        }
        else if ( flags & Color_Wheel::COLOR_LCH )
        {
            p->hue = old_col.hueF();
            p->sat = color_chromaF(old_col);
            p->val = color_lumaF(old_col);
            p->color_from = &color_from_lch;
            p->rainbow_from_hue = &rainbow_lch;
        }
        else
        {
            p->hue = old_col.hsvHueF();
            p->sat = old_col.hsvSaturationF();
            p->val = old_col.valueF();
            p->color_from = &QColor::fromHsvF;
            p->rainbow_from_hue = &rainbow_hsv;
        }
        p->render_ring();
    }

    p->display_flags = flags;
    p->render_inner_selector();
    update();
    emit displayFlagsChanged(flags);
}

Color_Wheel::Display_Flags Color_Wheel::displayFlags(Display_Flags mask) const
{
    return p->display_flags & mask;
}

void Color_Wheel::setDefaultDisplayFlags(Display_Flags flags)
{
    if ( !(flags & COLOR_FLAGS) )
        flags |= hard_default_flags & COLOR_FLAGS;
    if ( !(flags & ANGLE_FLAGS) )
        flags |= hard_default_flags & ANGLE_FLAGS;
    if ( !(flags & SHAPE_FLAGS) )
        flags |= hard_default_flags & SHAPE_FLAGS;
    default_flags = flags;
}

Color_Wheel::Display_Flags Color_Wheel::defaultDisplayFlags(Display_Flags mask)
{
    return default_flags & mask;
}

void Color_Wheel::setDisplayFlag(Display_Flags flag, Display_Flags mask)
{
    setDisplayFlags((p->display_flags&~mask)|flag);
}

