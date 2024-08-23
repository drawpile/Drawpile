/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef COLOR_WHEEL_HPP
#define COLOR_WHEEL_HPP

#include <QWidget>

#include "colorwidgets_global.hpp"


namespace color_widgets {

/**
 * \brief Display an analog widget that allows the selection of a HSV color
 *
 * It has an outer wheel to select the Hue and an intenal square to select
 * Saturation and Lightness.
 */
class QCP_EXPORT ColorWheel : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged DESIGNABLE true STORED false )
    Q_PROPERTY(qreal hue READ hue WRITE setHue DESIGNABLE false )
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation DESIGNABLE false )
    Q_PROPERTY(qreal value READ value WRITE setValue DESIGNABLE false )
    Q_PROPERTY(unsigned wheelWidth READ wheelWidth WRITE setWheelWidth NOTIFY wheelWidthChanged DESIGNABLE true )
    Q_PROPERTY(ShapeEnum selectorShape READ selectorShape WRITE setSelectorShape NOTIFY selectorShapeChanged DESIGNABLE true )
    Q_PROPERTY(bool rotatingSelector READ rotatingSelector WRITE setRotatingSelector NOTIFY rotatingSelectorChanged DESIGNABLE true )
    Q_PROPERTY(ColorSpaceEnum colorSpace READ colorSpace WRITE setColorSpace NOTIFY colorSpaceChanged DESIGNABLE true )
    Q_PROPERTY(bool mirroredSelector READ mirroredSelector WRITE setMirroredSelector NOTIFY mirroredSelectorChanged DESIGNABLE true )
    Q_PROPERTY(bool alignTop READ alignTop WRITE setAlignTop NOTIFY alignTop DESIGNABLE false )
    Q_PROPERTY(bool keepWheelRatio READ keepWheelRatio WRITE setKeepWheelRatio NOTIFY keepWheelRatioChanged DESIGNABLE false )
    Q_PROPERTY(bool previewOuter READ previewOuter WRITE setPreviewOuter NOTIFY previewOuterChanged DESIGNABLE false )
    Q_PROPERTY(bool previewInner READ previewInner WRITE setPreviewInner NOTIFY previewInnerChanged DESIGNABLE false )
    Q_PROPERTY(QColor comparisonColor READ comparisonColor WRITE setComparisonColor NOTIFY comparisonColorChanged DESIGNABLE false )

public:
    enum ShapeEnum
    {
        ShapeTriangle,  ///< A triangle
        ShapeSquare,    ///< A square
    };

    enum AngleEnum
    {
        AngleFixed,     ///< The inner part doesn't rotate
        AngleRotating,  ///< The inner part follows the hue selector
    };

    enum ColorSpaceEnum
    {
        ColorHSV,       ///< Use the HSV color space
        ColorHSL,       ///< Use the HSL color space
        ColorLCH,       ///< Use Luma Chroma Hue (Y_601')
    };

    Q_ENUM(ShapeEnum);
    Q_ENUM(AngleEnum);
    Q_ENUM(ColorSpaceEnum);

    explicit ColorWheel(QWidget *parent = nullptr);
    ~ColorWheel();

    /// Get current color
    QColor color() const;

    virtual QSize sizeHint() const Q_DECL_OVERRIDE;

    /// Get current hue in the range [0-1]
    qreal hue() const;

    /// Get current saturation in the range [0-1]
    qreal saturation() const;

    /// Get current value in the range [0-1]
    qreal value() const;

    /// Get the base width in pixels of the outer wheel
    unsigned int wheelWidth() const;

    /// Set the base width in pixels of the outer wheel
    void setWheelWidth(unsigned int w);

    /// Shape of the internal selector
    ShapeEnum selectorShape() const;

    /// Whether the internal selector should rotare in accordance with the hue
    bool rotatingSelector() const;

    /// Color space used to preview/edit the color
    ColorSpaceEnum colorSpace() const;

    /// Whether the internal selector's should be mirrored horizontally
    bool mirroredSelector() const;

    /// Whether the color wheel is aligned to the top or the center
    bool alignTop() const;

    /// Whether the outer wheel scales with the size of the widget or not
    bool keepWheelRatio() const;

    /// Whether to preview the color on the outer ring when picking on the inner selector
    bool previewOuter() const;

    /// Whether to preview the color inside the ring when picking on the ring
    bool previewInner() const;

    /// Color to compare to when previewing
    QColor comparisonColor() const;

public Q_SLOTS:

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

    /// Sets the shape of the internal selector
    void setSelectorShape(ShapeEnum shape);

    /// Sets whether the internal selector should rotare in accordance with the hue
    void setRotatingSelector(bool rotating);

    /// Sets the color space used to preview/edit the color
    void setColorSpace(ColorSpaceEnum space);

    /// Sets whether the internal selector should be mirrored horizontally
    void setMirroredSelector(bool mirrored);

    /// Sets whether the wheel should be aligned to the top or the center
    void setAlignTop(bool top);

    /// Whether to retain the wheel's width ratio as the widget scales or not
    void setKeepWheelRatio(bool keep);

    /// Whether to preview the color on the outer ring when picking on the inner selector
    void setPreviewOuter(bool preview);

    /// Whether to preview the color inside the ring when picking on the ring
    void setPreviewInner(bool preview);

    /// Color to compare to when previewing, set to an invalid color to disable
    void setComparisonColor(QColor color);

Q_SIGNALS:
    /**
     * Emitted when the user selects a color or setColor is called
     */
    void colorChanged(QColor);

    /**
     * Emitted when the user selects a color
     */
    void colorSelected(QColor);

    void wheelWidthChanged(unsigned);

    void selectorShapeChanged(ShapeEnum shape);

    void rotatingSelectorChanged(bool rotating);

    void colorSpaceChanged(ColorSpaceEnum space);

    void mirroredSelectorChanged(bool mirrored);

    void alignTopChanged(bool top);

    void keepWheelRatioChanged(bool keep);

    void previewOuterChanged(bool preview);

    void previewInnerChanged(bool preview);

    void comparisonColorChanged(QColor color);

    /**
     * Emitted when the user releases from dragging
     */
    void editingFinished();

protected:
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent* event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent* event) Q_DECL_OVERRIDE;

protected:
    class Private;
    ColorWheel(QWidget *parent, Private* data);
    Private* data() const { return p; }

private:
    Private * const p;

};

} // namespace color_widgets

#endif // COLOR_WHEEL_HPP
