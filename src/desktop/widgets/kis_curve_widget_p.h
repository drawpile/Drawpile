// Copyright (c) 2005 C. Boemann <cbo@boemann.dk>
// Copyright (c) 2009 Dmitry Kazakov <dimula73@gmail.com>
// Copyright (c) 2015 Calle Laakkonen (modifications for Drawpile)
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _KIS_CURVE_WIDGET_P_H_
#define _KIS_CURVE_WIDGET_P_H_

#include "libclient/utils/kis_cubic_curve.h"

enum enumState {
    ST_NORMAL,
    ST_DRAG
};

/**
 * Private members for KisCurveWidget class
 */
class KisCurveWidget::Private
{

    KisCurveWidget *m_curveWidget;


public:
	Private(KisCurveWidget *parent);

    /* Dragging variables */
    int m_grab_point_index;
    double m_grabOffsetX;
    double m_grabOffsetY;
    double m_grabOriginalX;
    double m_grabOriginalY;
    QPointF m_draggedAwayPoint;
    int m_draggedAwayPointIndex;

    bool m_readOnlyMode;
    bool m_guideVisible;
    QColor m_colorGuide;


    /* The curve itself */
    bool    m_splineDirty;
    KisCubicCurve m_curve;
	KisCubicCurve m_defaultcurve;

    QPixmap m_pix;
    QPixmap m_pixmapBase;
    bool m_pixmapDirty;
    QPixmap *m_pixmapCache;

    /* In/Out controls */
    QDoubleSpinBox *m_doubleIn;
    QDoubleSpinBox *m_doubleOut;

    /* Working range of them */
    double m_inMin;
    double m_inMax;
    double m_outMin;
    double m_outMax;

	/* Context menu */
	QMenu *m_ctxmenu;
	QAction *m_removeCurrentPointAction;

    /**
     * State functions.
     * At the moment used only for dragging.
     */
    enumState m_state;

    /* Drawpile patch */
    int m_mode = MODE_CUBIC;

    inline void setState(enumState st);
    inline enumState state() const;



    /*** Internal routines ***/

    /**
     * Common update routines
     */
    void setCurveModified();
    void setCurveRepaint();


    /**
     * Convert working range of
     * In/Out controls to normalized
     * range of spline (and reverse)
     */
    double in2sp(double x);
    double out2sp(double y);
    double sp2in(double x);
    double sp2out(double y);


    /**
     * Check whether newly created/moved point @pt doesn't overlap
     * with any of existing ones from @m_points and adjusts its coordinates.
     * @skipIndex is the index of the point, that shouldn't be taken
     * into account during the search
     * (e.g. beacuse it's @pt itself)
     *
     * Returns false in case the point can't be placed anywhere
     * without overlapping
     */
    bool jumpOverExistingPoints(QPointF &pt, int skipIndex);


    /**
     * Synchronize In/Out spinboxes with the curve
     */
    void syncIOControls();

    /**
     * Find the nearest point to @pt from m_points
     */
    int nearestPointInRange(QPointF pt, int wWidth, int wHeight) const;

    /**
     * Nothing to be said! =)
     */
    inline
	void drawGrid(QPainter &p, int wWidth, int wHeight);

};

KisCurveWidget::Private::Private(KisCurveWidget *parent)
{
    m_curveWidget = parent;
}

double KisCurveWidget::Private::in2sp(double x)
{
    double rangeLen = m_inMax - m_inMin;
    return (x - m_inMin) / rangeLen;
}

double KisCurveWidget::Private::out2sp(double y)
{
    double rangeLen = m_outMax - m_outMin;
    return (y - m_outMin) / rangeLen;
}

double KisCurveWidget::Private::sp2in(double x)
{
    double rangeLen = m_inMax - m_inMin;
    return x * rangeLen + m_inMin;
}

double KisCurveWidget::Private::sp2out(double y)
{
    double rangeLen = m_outMax - m_outMin;
    return y * rangeLen + m_outMin;
}


bool KisCurveWidget::Private::jumpOverExistingPoints(QPointF &pt, int skipIndex)
{
    foreach(const QPointF &it, m_curve.points()) {
        if (m_curve.points().indexOf(it) == skipIndex)
            continue;
        if (fabs(it.x() - pt.x()) < POINT_AREA)
            pt.rx() = pt.x() >= it.x() ?
                      it.x() + POINT_AREA : it.x() - POINT_AREA;
    }
    return (pt.x() >= 0 && pt.x() <= 1.);
}

int KisCurveWidget::Private::nearestPointInRange(QPointF pt, int wWidth, int wHeight) const
{
    double nearestDistanceSquared = 1000;
    int nearestIndex = -1;
    int i = 0;

    foreach(const QPointF & point, m_curve.points()) {
        double distanceSquared = (pt.x() - point.x()) *
                                 (pt.x() - point.x()) +
                                 (pt.y() - point.y()) *
                                 (pt.y() - point.y());

        if (distanceSquared < nearestDistanceSquared) {
            nearestIndex = i;
            nearestDistanceSquared = distanceSquared;
        }
        ++i;
    }

    if (nearestIndex >= 0) {
		if (fabs(pt.x() - m_curve.points()[nearestIndex].x()) *(wWidth - 1) < 10 &&
				fabs(pt.y() - m_curve.points()[nearestIndex].y()) *(wHeight - 1) < 10) {
            return nearestIndex;
        }
    }

    return -1;
}


template<typename T>
static inline constexpr auto div2_round(T x) {
    return (x+1)>>1;
}
template<typename T>
static inline constexpr auto div4_round(T x) {
    return (x+2)>>2;
}

void KisCurveWidget::Private::drawGrid(QPainter &p, int wWidth, int wHeight)
{
    /**
     * Hint: widget size should conform
     * formula 4n+5 to draw grid correctly
     * without curious shifts between
     * spline and it caused by rounding
     *
     * That is not mandatory but desirable
     */

    p.drawLine(div4_round(wWidth), 0, div4_round(wWidth), wHeight);
    p.drawLine(div2_round(wWidth), 0, div2_round(wWidth), wHeight);
    p.drawLine(div4_round(3*wWidth), 0, div4_round(3*wWidth), wHeight);

    p.drawLine(0, div4_round(wHeight), wWidth, div4_round(wHeight));
    p.drawLine(0, div2_round(wHeight), wWidth, div2_round(wHeight));
    p.drawLine(0, div4_round(3*wHeight), wWidth, div4_round(3*wHeight));

}

void KisCurveWidget::Private::syncIOControls()
{
    if (!m_doubleIn || !m_doubleOut)
        return;

    bool somethingSelected = (m_grab_point_index >= 0);

    m_doubleIn->setEnabled(somethingSelected);
    m_doubleOut->setEnabled(somethingSelected);

    if (m_grab_point_index >= 0) {
        m_doubleIn->blockSignals(true);
        m_doubleOut->blockSignals(true);

        m_doubleIn->setRange(m_inMin, m_inMax);
        m_doubleOut->setRange(m_outMin, m_outMax);
        m_doubleIn->setValue(sp2in(m_curve.points()[m_grab_point_index].x()));
        m_doubleOut->setValue(sp2out(m_curve.points()[m_grab_point_index].y()));

        m_doubleIn->blockSignals(false);
        m_doubleOut->blockSignals(false);
    } else {
        /*FIXME: Ideally, these controls should hide away now */
    }
}

void KisCurveWidget::Private::setCurveModified()
{
    syncIOControls();
    m_splineDirty = true;
    m_curveWidget->update();
    m_curveWidget->emit modified();
}

void KisCurveWidget::Private::setCurveRepaint()
{
    m_curveWidget->update();
}

void KisCurveWidget::Private::setState(enumState st)
{
    m_state = st;
}


enumState KisCurveWidget::Private::state() const
{
    return m_state;
}


#endif /* _KIS_CURVE_WIDGET_P_H_ */
