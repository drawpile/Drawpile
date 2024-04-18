/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef COLOR_SELECTOR_HPP
#define COLOR_SELECTOR_HPP

#include "color_preview.hpp"
#include "color_wheel.hpp"

namespace color_widgets {

/**
 * Color preview that opens a color dialog
 */
class QCP_EXPORT ColorSelector : public ColorPreview
{
    Q_OBJECT
    Q_ENUMS(UpdateMode)
    Q_PROPERTY(UpdateMode updateMode READ updateMode WRITE setUpdateMode NOTIFY updateModeChanged)
    Q_PROPERTY(Qt::WindowModality dialogModality READ dialogModality WRITE setDialogModality NOTIFY dialogModalityChanged)
    Q_PROPERTY(ColorWheel::ShapeEnum wheelShape READ wheelShape WRITE setWheelShape NOTIFY wheelShapeChanged)
    Q_PROPERTY(ColorWheel::ColorSpaceEnum colorSpace READ colorSpace WRITE setColorSpace NOTIFY colorSpaceChanged)
    Q_PROPERTY(bool wheelRotating READ wheelRotating WRITE setWheelRotating NOTIFY wheelRotatingChanged)

public:
    enum UpdateMode {
        Confirm, ///< Update color only after the dialog has been accepted
        Continuous ///< Update color as it's being modified in the dialog
    };

    explicit ColorSelector(QWidget *parent = nullptr);
    ~ColorSelector();

    void setUpdateMode(UpdateMode m);
    UpdateMode updateMode() const;

    Qt::WindowModality dialogModality() const;
    void setDialogModality(Qt::WindowModality m);

    ColorWheel::ShapeEnum wheelShape() const;
    ColorWheel::ColorSpaceEnum colorSpace() const;
    bool wheelRotating() const;

Q_SIGNALS:
    void wheelShapeChanged(ColorWheel::ShapeEnum shape);
    void colorSpaceChanged(ColorWheel::ColorSpaceEnum space);
    void wheelRotatingChanged(bool rotating);
    void updateModeChanged(UpdateMode);
    void dialogModalityChanged(Qt::WindowModality);
    /// Emitted when a color is selected by the user
    void colorSelected(const QColor& c);

public Q_SLOTS:
    void showDialog();
    void setWheelShape(ColorWheel::ShapeEnum shape);
    void setColorSpace(ColorWheel::ColorSpaceEnum space);
    void setWheelRotating(bool rotating);

private Q_SLOTS:
    void accept_dialog();
    void reject_dialog();
    void update_old_color(const QColor &c);

protected:
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent * event) Q_DECL_OVERRIDE;

private:
    /// Connect/Disconnect colorChanged based on UpdateMode
    void connect_dialog();

    /// Disconnect from dialog update
    void disconnect_dialog();

    class Private;
    Private * const p;
};

} // namespace color_widgets

#endif // COLOR_SELECTOR_HPP
