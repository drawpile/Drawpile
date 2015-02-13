/**

@author Calle Laakkonen

@section License

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
#ifndef HUE_SLIDER_HPP
#define HUE_SLIDER_HPP

#include "gradient_slider.hpp"

/**
 * \brief A slider for selecting a hue value
 */
class QCP_EXPORT Hue_Slider : public Gradient_Slider
{
    Q_OBJECT
    Q_PROPERTY(qreal colorSaturation READ colorSaturation WRITE setColorSaturation)
    Q_PROPERTY(qreal colorValue READ colorValue WRITE setColorValue)

public:
    explicit Hue_Slider(QWidget *parent = 0);
    explicit Hue_Slider(Qt::Orientation orientation, QWidget *parent = 0);
    ~Hue_Slider();

    qreal colorSaturation() const;
    void setColorSaturation(qreal value);

    qreal colorValue() const;
    void setColorValue(qreal value);

private:
    class Private;
    Private * const p;
};

#endif // HUE_SLIDER_HPP

