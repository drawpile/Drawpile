// Copyright (c) 2005 C. Boemann <cbo@boemann.dk>
// Copyright (c) 2009 Dmitry Kazakov <dimula73@gmail.com>
// Copyright (c) 2015 Calle Laakkonen (modifications for Drawpile)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>

#include <QPixmap>
#include <QPainter>
#include <QPoint>
#include <QPen>
#include <QEvent>
#include <QRect>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QList>
#include <QMenu>
#include <QSpinBox>
#include <QtMath>

#include "desktop/widgets/kis_curve_widget.h"

static constexpr int MOUSE_AWAY_THRES = 15;
static constexpr double POINT_AREA = 1E-4;

#include "desktop/widgets/kis_curve_widget_p.h"

KisCurveWidget::KisCurveWidget(QWidget *parent, Qt::WindowFlags f)
		: QWidget(parent, f), d(new KisCurveWidget::Private(this))
{
	setObjectName("KisCurveWidget");
	d->m_grab_point_index = -1;
	d->m_readOnlyMode   = false;
	d->m_guideVisible   = false;
	d->m_pixmapDirty = true;
	d->m_pixmapCache = nullptr;
	d->setState(ST_NORMAL);

	d->m_intIn = nullptr;
	d->m_intOut = nullptr;

	d->m_ctxmenu = new QMenu(this);
	d->m_removeCurrentPointAction = d->m_ctxmenu->addAction(tr("Remove point"));
	QAction *resetAction = d->m_ctxmenu->addAction(tr("Reset"));

	connect(d->m_removeCurrentPointAction, &QAction::triggered, this, &KisCurveWidget::removeCurrentPoint);
	connect(resetAction, &QAction::triggered, this, &KisCurveWidget::reset);

	setMouseTracking(true);
	setAutoFillBackground(false);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setMinimumSize(50, 50);

	d->setCurveModified();

	setFocusPolicy(Qt::StrongFocus);
}

KisCurveWidget::~KisCurveWidget()
{
	delete d->m_pixmapCache;
	delete d;
}

void KisCurveWidget::setupInOutControls(QSpinBox *in, QSpinBox *out, int min, int max)
{
	d->m_intIn = in;
	d->m_intOut = out;

	if (!d->m_intIn || !d->m_intOut)
		return;

	d->m_inOutMin = min;
	d->m_inOutMax = max;

	d->m_intIn->setRange(d->m_inOutMin, d->m_inOutMax);
	d->m_intOut->setRange(d->m_inOutMin, d->m_inOutMax);


	connect(d->m_intIn, SIGNAL(valueChanged(int)), this, SLOT(inOutChanged(int)));
	connect(d->m_intOut, SIGNAL(valueChanged(int)), this, SLOT(inOutChanged(int)));
	d->syncIOControls();

}
void KisCurveWidget::dropInOutControls()
{
	if (!d->m_intIn || !d->m_intOut)
		return;

	disconnect(d->m_intIn, SIGNAL(valueChanged(int)), this, SLOT(inOutChanged(int)));
	disconnect(d->m_intOut, SIGNAL(valueChanged(int)), this, SLOT(inOutChanged(int)));

	d->m_intIn = d->m_intOut = nullptr;

}

void KisCurveWidget::inOutChanged(int)
{
	QPointF pt;

	Q_ASSERT(d->m_grab_point_index >= 0);

	pt.setX(d->io2sp(d->m_intIn->value()));
	pt.setY(d->io2sp(d->m_intOut->value()));

	if (d->jumpOverExistingPoints(pt, d->m_grab_point_index)) {
		d->m_curve.setPoint(d->m_grab_point_index, pt);
		d->m_grab_point_index = d->m_curve.points().indexOf(pt);
	} else
		pt = d->m_curve.points()[d->m_grab_point_index];


	d->m_intIn->blockSignals(true);
	d->m_intOut->blockSignals(true);

	d->m_intIn->setValue(d->sp2io(pt.x()));
	d->m_intOut->setValue(d->sp2io(pt.y()));

	d->m_intIn->blockSignals(false);
	d->m_intOut->blockSignals(false);

	d->setCurveModified();
	emit curveChanged(curve());
}


void KisCurveWidget::reset()
{
	d->m_grab_point_index = -1;
	d->m_guideVisible = false;

	setCurve(d->m_defaultcurve);
}

void KisCurveWidget::removeCurrentPoint()
{
	if (d->m_grab_point_index > 0 && d->m_grab_point_index < d->m_curve.points().count() - 1) {
		//x() find closest point to get focus afterwards
		double grab_point_x = d->m_curve.points()[d->m_grab_point_index].x();

		int left_of_grab_point_index = d->m_grab_point_index - 1;
		int right_of_grab_point_index = d->m_grab_point_index + 1;
		int new_grab_point_index;

		if (fabs(d->m_curve.points()[left_of_grab_point_index].x() - grab_point_x) <
				fabs(d->m_curve.points()[right_of_grab_point_index].x() - grab_point_x)) {
			new_grab_point_index = left_of_grab_point_index;
		} else {
			new_grab_point_index = d->m_grab_point_index;
		}
		d->m_curve.removePoint(d->m_grab_point_index);
		d->m_grab_point_index = new_grab_point_index;
		setCursor(Qt::ArrowCursor);
		d->setState(ST_NORMAL);
	}
	d->setCurveModified();
	emit curveChanged(curve());
}

void KisCurveWidget::setCurveGuide(const QColor & color)
{
	d->m_guideVisible = true;
	d->m_colorGuide   = color;

}

void KisCurveWidget::setPixmap(const QPixmap & pix)
{
	d->m_pix = pix;
	d->m_pixmapDirty = true;
	d->setCurveRepaint();
}

QPixmap KisCurveWidget::getPixmap()
{
	return d->m_pix;
}

void KisCurveWidget::setBasePixmap(const QPixmap &pix)
{
	d->m_pixmapBase = pix;
}

QPixmap KisCurveWidget::getBasePixmap()
{
	return d->m_pixmapBase;
}

void KisCurveWidget::setReadOnly(bool readOnly)
{
    d->m_readOnlyMode = readOnly;
}

bool KisCurveWidget::readOnly() const
{
    return d->m_readOnlyMode;
}

void KisCurveWidget::setLinear(bool linear)
{
	d->m_linear = linear;
}

bool KisCurveWidget::linear() const
{
	return d->m_linear;
}

void KisCurveWidget::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
		removeCurrentPoint();
	} else if (e->key() == Qt::Key_Escape && d->state() != ST_NORMAL) {
		d->m_curve.setPoint(d->m_grab_point_index, QPointF(d->m_grabOriginalX, d->m_grabOriginalY) );
		setCursor(Qt::ArrowCursor);
		d->setState(ST_NORMAL);

		d->setCurveModified();
		emit curveChanged(curve());
	} else if ((e->key() == Qt::Key_A || e->key() == Qt::Key_Insert) && d->state() == ST_NORMAL) {
		/* FIXME: Lets user choose the hotkeys */
		addPointInTheMiddle();
	} else
		QWidget::keyPressEvent(e);
}

void KisCurveWidget::addPointInTheMiddle()
{
	QPointF pt(0.5, d->m_curve.value(0.5));

	if (!d->jumpOverExistingPoints(pt, -1))
		return;

	d->m_grab_point_index = d->m_curve.addPoint(pt);

	if (d->m_intIn)
		d->m_intIn->setFocus(Qt::TabFocusReason);
	d->setCurveModified();
	emit curveChanged(curve());
}

void KisCurveWidget::resizeEvent(QResizeEvent *e)
{
	d->m_pixmapDirty = true;
	QWidget::resizeEvent(e);
}

void KisCurveWidget::paintEvent(QPaintEvent *)
{
	int    wWidth = width() - 1;
	int    wHeight = height() - 1;

	QPainter p(this);

	// Antialiasing is not a good idea here, because
	// the grid will drift one pixel to any side due to rounding of int
	// FIXME: let's user tell the last word (in config)
	//p.setRenderHint(QPainter::Antialiasing);

	//  draw background
	if (!d->m_pix.isNull()) {
		if (d->m_pixmapDirty || !d->m_pixmapCache) {
			delete d->m_pixmapCache;
			d->m_pixmapCache = new QPixmap(width(), height());
			QPainter cachePainter(d->m_pixmapCache);

			cachePainter.scale(1.0*width() / d->m_pix.width(), 1.0*height() / d->m_pix.height());
			cachePainter.drawPixmap(0, 0, d->m_pix);
			d->m_pixmapDirty = false;
		}
		p.drawPixmap(0, 0, *d->m_pixmapCache);

	} else {
		p.fillRect(rect(), palette().window());
	}

	p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::SolidLine));
	d->drawGrid(p, wWidth, wHeight);
	p.drawRect(QRect(0, 0, wWidth, wHeight));

	p.setRenderHint(QPainter::Antialiasing);

	// Draw curve.
	double curY;
	double normalizedX;
	int x;

	QPolygonF poly;
	// Drawpile patch: linear graph for MyPaint brush settings.
	if(d->m_linear) {
		QList<QPointF> points = d->m_curve.points();
		int count = points.count();
		if(count == 0) {
			poly.append(QPointF(0.0, wHeight));
			poly.append(QPointF(wWidth, 0.0));
		} else {
			poly.append(QPointF(0.0, (1.0 - points.first().y()) * wHeight));
			for(int i = 0; i < count; ++i) {
				poly.append(QPointF(points[i].x() * wWidth, (1.0 - points[i].y()) * wHeight));
			}
			poly.append(QPointF(wWidth, (1.0 - points.last().y()) * wHeight));
		}
	} else {
		p.setPen(QPen(palette().color(QPalette::WindowText), 1, Qt::SolidLine));
		for (x = 0 ; x < wWidth ; x++) {
			normalizedX = double(x) / wWidth;
			curY = wHeight - d->m_curve.value(normalizedX) * wHeight;

			/**
			* Keep in mind that QLineF rounds doubles
			* to ints mathematically, not just rounds down
			* like in C
			*/
			poly.append(QPointF(x, curY));
		}
		poly.append(QPointF(x, wHeight - d->m_curve.value(1.0) * wHeight));
	}
	p.drawPolyline(poly);

	// Drawing curve handles.
	double curveX;
	double curveY;
	if (!d->m_readOnlyMode) {
		for (int i = 0; i < d->m_curve.points().count(); ++i) {
			curveX = d->m_curve.points().at(i).x();
			curveY = d->m_curve.points().at(i).y();

			if (i == d->m_grab_point_index) {
				p.setPen(Qt::NoPen);
				p.setBrush(palette().color(QPalette::Highlight));
				p.drawEllipse(QRectF(curveX * wWidth - 6,
									 wHeight - 6 - curveY * wHeight, 12, 12));
			} else {
				//p.setPen(QPen(Qt::red, 1, Qt::SolidLine));
				p.setPen(QPen(palette().color(QPalette::Highlight), 3, Qt::SolidLine));
				p.setBrush(Qt::NoBrush);

				p.drawEllipse(QRectF(curveX * wWidth - 5,
									 wHeight - 5 - curveY * wHeight, 10, 10));
			}
		}
	}
}

void KisCurveWidget::mousePressEvent(QMouseEvent * e)
{
	if (d->m_readOnlyMode) return;

	if (e->button() != Qt::LeftButton)
		return;

	double x = e->pos().x() / double(width() - 1);
	double y = 1.0 - e->pos().y() / double(height() - 1);

	int closest_point_index = d->nearestPointInRange(QPointF(x, y), width(), height());
	if (closest_point_index < 0) {
		QPointF newPoint(x, y);
		if (!d->jumpOverExistingPoints(newPoint, -1))
			return;
		d->m_grab_point_index = d->m_curve.addPoint(newPoint);
	} else {
		d->m_grab_point_index = closest_point_index;
	}

	d->m_grabOriginalX = d->m_curve.points()[d->m_grab_point_index].x();
	d->m_grabOriginalY = d->m_curve.points()[d->m_grab_point_index].y();
	d->m_grabOffsetX = d->m_curve.points()[d->m_grab_point_index].x() - x;
	d->m_grabOffsetY = d->m_curve.points()[d->m_grab_point_index].y() - y;
	d->m_curve.setPoint(d->m_grab_point_index, QPointF(x + d->m_grabOffsetX, y + d->m_grabOffsetY));

	d->m_draggedAwayPointIndex = -1;
	d->setState(ST_DRAG);


	d->setCurveModified();
}


void KisCurveWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if (d->m_readOnlyMode) return;

	if (e->button() != Qt::LeftButton)
		return;

	setCursor(Qt::ArrowCursor);
	d->setState(ST_NORMAL);

	d->setCurveModified();
	emit curveChanged(curve());
}

void KisCurveWidget::contextMenuEvent(QContextMenuEvent *e)
{
	double x = e->pos().x() / double(width() - 1);
	double y = 1.0 - e->pos().y() / double(height() - 1);

	int closest_point_index = d->nearestPointInRange(QPointF(x, y), width(), height());
	if (closest_point_index >= 0) {
		d->m_grab_point_index = closest_point_index;
	} else
		d->m_grab_point_index = -1;

	d->m_removeCurrentPointAction->setEnabled(d->m_grab_point_index > 0 && d->m_grab_point_index < d->m_curve.points().count() - 1);
	d->m_ctxmenu->popup(e->globalPos());
}


void KisCurveWidget::mouseMoveEvent(QMouseEvent * e)
{
	if (d->m_readOnlyMode) return;

	double x = e->pos().x() / double(width() - 1);
	double y = 1.0 - e->pos().y() / double(height() - 1);

	if (d->state() == ST_NORMAL) { // If no point is selected set the cursor shape if on top
		int nearestPointIndex = d->nearestPointInRange(QPointF(x, y), width(), height());

		if (nearestPointIndex < 0)
			setCursor(Qt::ArrowCursor);
		else
			setCursor(Qt::CrossCursor);
	} else { // Else, drag the selected point
		bool crossedHoriz = e->pos().x() - width() > MOUSE_AWAY_THRES ||
							e->pos().x() < -MOUSE_AWAY_THRES;
		bool crossedVert =  e->pos().y() - height() > MOUSE_AWAY_THRES ||
							e->pos().y() < -MOUSE_AWAY_THRES;

		bool removePoint = (crossedHoriz || crossedVert);

		if (!removePoint && d->m_draggedAwayPointIndex >= 0) {
			// point is no longer dragged away so reinsert it
			QPointF newPoint(d->m_draggedAwayPoint);
			d->m_grab_point_index = d->m_curve.addPoint(newPoint);
			d->m_draggedAwayPointIndex = -1;
		}

		if (removePoint &&
				(d->m_draggedAwayPointIndex >= 0))
			return;


		setCursor(Qt::CrossCursor);

		x += d->m_grabOffsetX;
		y += d->m_grabOffsetY;

		double leftX;
		double rightX;
		if (d->m_grab_point_index == 0) {
			leftX = 0.0;
			if (d->m_curve.points().count() > 1)
				rightX = d->m_curve.points()[d->m_grab_point_index + 1].x() - POINT_AREA;
			else
				rightX = 1.0;
		} else if (d->m_grab_point_index == d->m_curve.points().count() - 1) {
			leftX = d->m_curve.points()[d->m_grab_point_index - 1].x() + POINT_AREA;
			rightX = 1.0;
		} else {
			Q_ASSERT(d->m_grab_point_index > 0 && d->m_grab_point_index < d->m_curve.points().count() - 1);

			// the 1E-4 addition so we can grab the dot later.
			leftX = d->m_curve.points()[d->m_grab_point_index - 1].x() + POINT_AREA;
			rightX = d->m_curve.points()[d->m_grab_point_index + 1].x() - POINT_AREA;
		}

		x = qBound(leftX, x, rightX);
		y = qBound(0.0, y, 1.0);

		d->m_curve.setPoint(d->m_grab_point_index, QPointF(x, y));

		if (removePoint && d->m_curve.points().count() > 2) {
			d->m_draggedAwayPoint = d->m_curve.points()[d->m_grab_point_index];
			d->m_draggedAwayPointIndex = d->m_grab_point_index;
			d->m_curve.removePoint(d->m_grab_point_index);
			d->m_grab_point_index = qBound(0, d->m_grab_point_index, d->m_curve.points().count() - 1);
		}

		d->setCurveModified();
	}
}

KisCubicCurve KisCurveWidget::curve()
{
	return d->m_curve;
}

void KisCurveWidget::setDefaultCurve(KisCubicCurve curve)
{
	d->m_defaultcurve = curve;
}

void KisCurveWidget::setCurve(KisCubicCurve inlist)
{
	d->m_curve = inlist;
	d->m_grab_point_index = qBound(0, d->m_grab_point_index, d->m_curve.points().count() - 1);
	d->setCurveModified();
	emit curveChanged(curve());
}

void KisCurveWidget::leaveEvent(QEvent *)
{
}

