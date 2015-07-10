/**

@author Mattia Basaglia

@section License

    Copyright (C) 2013-2015 Mattia Basaglia
    Copyright (C) 2014 Calle Laakkonen

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

#include "color_preview.hpp"

#include <QStylePainter>
#include <QStyleOptionFrame>
#include "paint_border.hpp"
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>

class Color_Preview::Private
{
public:
    QColor col; ///< color to be viewed
    QColor comparison; ///< comparison color
    QBrush back;///< Background brush, visible on transaprent color
    Display_Mode display_mode; ///< How the color(s) are to be shown

    Private() : col(Qt::red), back(Qt::darkGray, Qt::DiagCrossPattern), display_mode(NoAlpha)
    {}
};

Color_Preview::Color_Preview(QWidget *parent) :
    QWidget(parent), p(new Private)
{
    p->back.setTexture(QPixmap(QLatin1String(":/color_widgets/alphaback.png")));
}

Color_Preview::~Color_Preview()
{
    delete p;
}

void Color_Preview::setBackground(const QBrush &bk)
{
    p->back = bk;
    update();
}

QBrush Color_Preview::background() const
{
    return p->back;
}

Color_Preview::Display_Mode Color_Preview::displayMode() const
{
    return p->display_mode;
}

void Color_Preview::setDisplayMode(Display_Mode m)
{
    p->display_mode = m;
    update();
}

QColor Color_Preview::color() const
{
    return p->col;
}

QColor Color_Preview::comparisonColor() const
{
    return p->comparison;
}

QSize Color_Preview::sizeHint() const
{
    return QSize(24,24);
}

void Color_Preview::paint(QPainter &painter, QRect rect) const
{
    QColor c1, c2;
    switch(p->display_mode) {
    case NoAlpha:
        c1 = c2 = p->col.rgb();
        break;
    case AllAlpha:
        c1 = c2 = p->col;
        break;
    case SplitAlpha:
        c1 = p->col.rgb();
        c2 = p->col;
        break;
    case SplitColor:
        c1 = p->comparison;
        c2 = p->col;
        break;
    }

    if(c1.alpha()<255 || c2.alpha()<255)
        painter.fillRect(1, 1, rect.width()-2, rect.height()-2, p->back);

    int w = (rect.width() - 2) / 2;
    int h = rect.height() - 2;
    painter.fillRect(1, 1, w, h, c1);
    painter.fillRect(1+w, 1, w, h, c2);

    paint_tl_border(painter,size(),palette().color(QPalette::Mid),0);
    paint_tl_border(painter,size(),palette().color(QPalette::Dark),1);

    paint_br_border(painter,size(),palette().color(QPalette::Midlight),1);
    paint_br_border(painter,size(),palette().color(QPalette::Button),0);
}

void Color_Preview::setColor(const QColor &c)
{
    p->col = c;
    update();
    emit colorChanged(c);
}

void Color_Preview::setComparisonColor(const QColor &c)
{
    p->comparison = c;
    update();
}

void Color_Preview::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);

    paint(painter, geometry());
}

void Color_Preview::resizeEvent(QResizeEvent *)
{
    update();
}

void Color_Preview::mouseReleaseEvent(QMouseEvent * ev)
{
    if ( QRect(QPoint(0,0),size()).contains(ev->pos()) )
        emit clicked();
}

void Color_Preview::mouseMoveEvent(QMouseEvent *ev)
{

    if ( ev->buttons() &Qt::LeftButton && !QRect(QPoint(0,0),size()).contains(ev->pos()) )
    {
        QMimeData *data = new QMimeData;

        data->setColorData(p->col);

        QDrag* drag = new QDrag(this);
        drag->setMimeData(data);

        QPixmap preview(24,24);
        preview.fill(p->col);
        drag->setPixmap(preview);

        drag->exec();
    }
}

