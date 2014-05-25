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
#ifndef PAINT_BORDER_HPP
#define PAINT_BORDER_HPP

#include <QWidget>
#include <QPainter>

inline void paint_tl_border(QPainter &painter, QSize sz, QColor c, int inpx)
{
    //c.setAlpha(0.5);
    painter.setPen(c);
    painter.drawLine(inpx,inpx,sz.width()-1-inpx,inpx);
    painter.drawLine(inpx,inpx,inpx,sz.height()-1-inpx);
}

inline void paint_br_border(QPainter &painter, QSize sz, QColor c, int inpx)
{
    //c.setAlpha(0.5);
    painter.setPen(c);
    painter.drawLine(sz.width()-1-inpx, inpx,
                     sz.width()-1-inpx, sz.height()-1-inpx );
    painter.drawLine(inpx, sz.height()-1-inpx,
                     sz.width()-1-inpx, sz.height()-1-inpx );
}

#endif // PAINT_BORDER_HPP
