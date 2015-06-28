/**

@author Mattia Basaglia

@section License

    Copyright (C) 2013-2015 Mattia Basaglia

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
#ifndef COLOR_WHEEL_HPP
#define COLOR_WHEEL_HPP

#include "colorpicker_global.hpp"

#include <QWidget>

/**
 * \brief Display an analog widget that allows the selection of a HSV color
 *
 * It has an outer wheel to select the Hue and an intenal square to select
 * Saturation and Lightness.
 */
class QCP_EXPORT Color_Wheel : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged DESIGNABLE true STORED false )
    Q_PROPERTY(qreal hue READ hue WRITE setHue DESIGNABLE false )
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation DESIGNABLE false )
    Q_PROPERTY(qreal value READ value WRITE setValue DESIGNABLE false )
    Q_PROPERTY(unsigned wheelWidth READ wheelWidth WRITE setWheelWidth DESIGNABLE true )
    Q_PROPERTY(Display_Flags displayFlags READ displayFlags WRITE setDisplayFlags NOTIFY displayFlagsChanged DESIGNABLE true )

public:
    enum Display_Enum
    {
        SHAPE_DEFAULT  = 0x000, ///< Use the default shape
        SHAPE_TRIANGLE = 0x001, ///< A triangle
        SHAPE_SQUARE   = 0x002, ///< A square
        SHAPE_FLAGS    = 0x00f, ///< Mask for the shape flags

        ANGLE_DEFAULT  = 0x000, ///< Use the default rotation style
        ANGLE_FIXED    = 0x010, ///< The inner part doesn't rotate
        ANGLE_ROTATING = 0x020, ///< The inner part follows the hue selector
        ANGLE_FLAGS    = 0x0f0, ///< Mask for the angle flags

        COLOR_DEFAULT  = 0x000, ///< Use the default colorspace
        COLOR_HSV      = 0x100, ///< Use the HSV color space
        COLOR_HSL      = 0x200, ///< Use the HSL color space
        COLOR_LCH      = 0x400, ///< Use Luma Chroma Hue (Y_601')
        COLOR_FLAGS    = 0xf00, ///< Mask for the color space flags

        FLAGS_DEFAULT  = 0x000, ///< Use all defaults
        FLAGS_ALL      = 0xfff  ///< Mask matching all flags
    };
    Q_DECLARE_FLAGS(Display_Flags, Display_Enum)
    Q_FLAGS(Display_Flags)

    explicit Color_Wheel(QWidget *parent = 0);
    ~Color_Wheel();

    /// Get current color
    QColor color() const;

    QSize sizeHint() const;

    /// Get current hue in the range [0-1]
    qreal hue() const;

    /// Get current saturation in the range [0-1]
    qreal saturation() const;

    /// Get current value in the range [0-1]
    qreal value() const;

    /// Get the width in pixels of the outer wheel
    unsigned int wheelWidth() const;

    /// Set the width in pixels of the outer wheel
    void setWheelWidth(unsigned int w);

    /// Get display flags
    Display_Flags displayFlags(Display_Flags mask = FLAGS_ALL) const;

    /// Set the default display flags
    static void setDefaultDisplayFlags(Display_Flags flags);

    /// Get default display flags
    static Display_Flags defaultDisplayFlags(Display_Flags mask = FLAGS_ALL);

    /**
     * @brief Set a specific display flag
     * @param flag  Flag replacing the mask
     * @param mask  Mask to be cleared
     */
    void setDisplayFlag(Display_Flags flag, Display_Flags mask);

public slots:

    /// Set current color
    void setColor(QColor c);

    /**
     * @param h Hue [0-1]
     */
    void setHue(qreal h);

    /**
     * @param s Saturation [0-1]
     */
    void setSaturation(qreal s);

    /**
     * @param v Value [0-1]
     */
    void setValue(qreal v);

    /**
     * @brief Set the display flags
     * @param flags which will replace the current ones
     */
    void setDisplayFlags(Color_Wheel::Display_Flags flags);

signals:
    /**
     * Emitted when the user selects a color or setColor is called
     */
    void colorChanged(QColor);

    /**
     * Emitted when the user selects a color
     */
    void colorSelected(QColor);

    void displayFlagsChanged(Color_Wheel::Display_Flags flags);

protected:
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void resizeEvent(QResizeEvent *);

private:
    class Private;
    Private * const p;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Color_Wheel::Display_Flags)

#endif // COLOR_WHEEL_HPP
