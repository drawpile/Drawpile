// Copyright (c) 2005 C. Boemann <cbo@boemann.dk>
// Copyright (c) 2009 Dmitry Kazakov <dimula73@gmail.com>
// Copyright (c) 2015 Calle Laakkonen (modifications for Drawpile)
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KIS_CURVE_WIDGET_H
#define KIS_CURVE_WIDGET_H

#include <QWidget>
#include <QColor>
#include <QPointF>
#include <QPixmap>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QPaintEvent>
#include <QList>

class QSpinBox;
class KisCubicCurve;

/**
 * KisCurveWidget is a widget that shows a single curve that can be edited
 * by the user. The user can grab the curve and move it; this creates
 * a new control point. Control points can be deleted by selecting a point
 * and pressing the delete key.
 *
 * (From: http://techbase.kde.org/Projects/Widgets_and_Classes#KisCurveWidget)
 * KisCurveWidget allows editing of spline based y=f(x) curves. Handy for cases
 * where you want the user to control such things as tablet pressure
 * response, color transformations, acceleration by time, aeroplane lift
 *by angle of attack.
 */
class KisCurveWidget final : public QWidget
{
    Q_OBJECT

public:

    /**
     * Create a new curve widget with a default curve, that is a straight
     * line from bottom-left to top-right.
     */
	KisCurveWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	~KisCurveWidget() override;

    /**
     * Enable the guide and set the guide color to the specified color.
     *
     * XXX: it seems that the guide feature isn't actually implemented yet?
     */
    void setCurveGuide(const QColor & color);

	/**
	 * Set the default shape
	 */
	void setDefaultCurve(KisCubicCurve curve);

    /**
     * Set a background pixmap. The background pixmap will be drawn under
     * the grid and the curve.
     *
     * XXX: or is the pixmap what is drawn to the  left and bottom of the curve
     * itself?
     */
    void setPixmap(const QPixmap & pix);
    QPixmap getPixmap();

    void setBasePixmap(const QPixmap & pix);
    QPixmap getBasePixmap();

    void setLinear(bool linear);
    bool linear() const;

public slots:
	/**
	 * Reset the curve to the default shape
	 */
	void reset();

signals:

    /**
     * Emitted whenever a control point has changed position.
     */
    void modified(void);

	/**
	 * Emitted when curve editing has finished
	 */
	void curveChanged(const KisCubicCurve &curve);

protected slots:
    void inOutChanged(int);
	void removeCurrentPoint();

protected:

	void keyPressEvent(QKeyEvent *) override;
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent * e) override;
	void mouseReleaseEvent(QMouseEvent * e) override;
	void mouseMoveEvent(QMouseEvent * e) override;
	void contextMenuEvent(QContextMenuEvent * e) override;
	void leaveEvent(QEvent *) override;
	void resizeEvent(QResizeEvent *e) override;

public:

    /**
     * @return get a list with all defined points. If you want to know what the
     * y value for a given x is on the curve defined by these points, use getCurveValue().
     * @see getCurveValue
     */
    KisCubicCurve curve();

    /**
     * Replace the current curve with a curve specified by the curve defined by the control
     * points in @param inlist.
     */
    void setCurve(KisCubicCurve inlist);

    /**
     * Connect/disconnect external spinboxes to the curve
     * @min/@max - is the range for their values
     */
    void setupInOutControls(QSpinBox *in, QSpinBox *out, int min, int max);
    void dropInOutControls();

    /**
     * Handy function that creates new point in the middle
     * of the curve and sets focus on the m_intIn field,
     * so the user can move this point anywhere in a moment
     */
    void addPointInTheMiddle();

private:

    class Private;
    Private * const d;

};


#endif /* KIS_CURVE_WIDGET_H */
