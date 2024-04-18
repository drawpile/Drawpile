/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef COLOR_LIST_WIDGET_HPP
#define COLOR_LIST_WIDGET_HPP

#include "abstract_widget_list.hpp"
#include "color_wheel.hpp"

namespace color_widgets {

class QCP_EXPORT ColorListWidget : public AbstractWidgetList
{
    Q_OBJECT

    Q_PROPERTY(QList<QColor> colors READ colors WRITE setColors NOTIFY colorsChanged )
    Q_PROPERTY(ColorWheel::ShapeEnum wheelShape READ wheelShape WRITE setWheelShape NOTIFY wheelShapeChanged)
    Q_PROPERTY(ColorWheel::ColorSpaceEnum colorSpace READ colorSpace WRITE setColorSpace NOTIFY colorSpaceChanged)
    Q_PROPERTY(bool wheelRotating READ wheelRotating WRITE setWheelRotating NOTIFY wheelRotatingChanged)

public:
    explicit ColorListWidget(QWidget *parent = nullptr);
    ~ColorListWidget();

    QList<QColor> colors() const;
    void setColors(const QList<QColor>& colors);

    void swap(int a, int b) Q_DECL_OVERRIDE;

    void append() Q_DECL_OVERRIDE;

    ColorWheel::ShapeEnum wheelShape() const;
    ColorWheel::ColorSpaceEnum colorSpace() const;
    bool wheelRotating() const;

Q_SIGNALS:
    void colorsChanged(const QList<QColor>&);
    void wheelShapeChanged(ColorWheel::ShapeEnum shape);
    void colorSpaceChanged(ColorWheel::ColorSpaceEnum space);
    void wheelRotatingChanged(bool rotating);

public Q_SLOTS:
    void setWheelShape(ColorWheel::ShapeEnum shape);
    void setColorSpace(ColorWheel::ColorSpaceEnum space);
    void setWheelRotating(bool rotating);

private Q_SLOTS:
    void emit_changed();
    void handle_removed(int);
    void color_changed(int row);

private:
    class Private;
    Private * const p;
    void  append_widget(int col);
};

} // namespace color_widgets

#endif // COLOR_LIST_WIDGET_HPP
