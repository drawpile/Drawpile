// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/mypaintinput.h"
#include "libclient/brushes/brush.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QtMath>
#include <QtGlobal>

namespace widgets {

MyPaintInput::MyPaintInput(
	const QString &title, const QString &description,
	const MyPaintBrushSettingInfo *settingInfo,
	const MyPaintBrushInputInfo *inputInfo, QWidget *parent)
	: QWidget{parent}
	// Some values are "unlimited", using -FLT_MAX and FLT_MAX as their range.
	// In those cases MyPaint uses -20 and 20 as the limits, so we do too.
	, m_xHardMax{inputInfo->hard_max > 1e6 ? 20.0 : inputInfo->hard_max}
	, m_xHardMin{inputInfo->hard_min < -1e6 ? -20.0 : inputInfo->hard_min}
	, m_yHardMax{settingInfo->max - settingInfo->min}
	, m_yHardMin{-m_yHardMax}
	, m_xSoftMax{inputInfo->soft_max}
	, m_xSoftMin{inputInfo->soft_min}
	// Arbitrary default limits, seems to work okay.
	, m_ySoftMax{m_yHardMax / 4.0}
	, m_ySoftMin{-m_ySoftMax}
{
	QVBoxLayout *widgetLayout = new QVBoxLayout;
	setLayout(widgetLayout);
	widgetLayout->setContentsMargins(0, 0, 0, 0);

	m_box = new QCheckBox{title, this};
	widgetLayout->addWidget(m_box);
	m_box->setToolTip(description);
	connect(
		m_box, &QCheckBox::stateChanged, this, &MyPaintInput::changeBoxState);

	// Constructing these widgets in large volumes is slow and make the brush
	// setting dialog take a long time to appear, so we lazily initialize them.
	m_curveWrapper = nullptr;
	m_ySpinner = nullptr;
	m_xMinSpinner = nullptr;
	m_xMaxSpinner = nullptr;
	m_yMaxLabel = nullptr;
	m_yMinLabel = nullptr;
	m_xMaxLabel = nullptr;
	m_xMinLabel = nullptr;
	m_curve = nullptr;
}

MyPaintInput::~MyPaintInput() {}

DP_MyPaintControlPoints MyPaintInput::controlPoints() const
{

	DP_MyPaintControlPoints cps = {{0}, {0}, 0};
	if(m_curve) {
		QList<QPointF> points = m_curve->curve().points();
		if(m_box->isChecked() && points.size() > 1) {
			// Get rid of any excess points. We'll keep removing from the middle
			// and hope that those are less important than the extremes.
			while(points.size() > DP_MYPAINT_CONTROL_POINTS_COUNT) {
				points.removeAt(points.size() / 2);
			}
			cps.n = points.size();
			for(int i = 0; i < cps.n; ++i) {
				const QPointF &point = curveToControlPoint(points[i]);
				cps.xvalues[i] = point.x();
				cps.yvalues[i] = point.y();
			}
		}
	}
	return cps;
}

void MyPaintInput::setControlPoints(const DP_MyPaintControlPoints &cps)
{
	bool haveCurve = !isNullControlPoints(cps);

	brushes::MyPaintCurve curve;
	curve.visible = haveCurve;
	if(haveCurve) {
		curve.xMax = curve.xMin = cps.xvalues[0];
		curve.yMax = curve.yMin = cps.yvalues[0];
		for(int i = 1; i < cps.n; ++i) {
			double x = cps.xvalues[i];
			if(x > curve.xMax) {
				curve.xMax = x;
			}
			if(x < curve.xMin) {
				curve.xMin = x;
			}
			double y = cps.yvalues[i];
			if(y > curve.yMax) {
				curve.yMax = y;
			}
			if(y < curve.yMin) {
				curve.yMin = y;
			}
		}

		double yAbs = qMax(qAbs(curve.yMin), qAbs(curve.yMax));
		curve.yMin = -yAbs;
		curve.yMax = yAbs;

		QList<QPointF> points;
		for(int i = 0; i < cps.n; ++i) {
			points.append(controlPointToCurve(
				curve, QPointF{cps.xvalues[i], cps.yvalues[i]}));
		}
		curve.curve.setPoints(points);
	} else {
		curve.xMax = m_xSoftMax;
		curve.xMin = m_xSoftMin;
		curve.yMax = m_ySoftMax;
		curve.yMin = m_ySoftMin;
		curve.curve.setPoints({{0.0, 0.25}, {1.0, 0.75}});
	}

	setMyPaintCurve(curve);
}

brushes::MyPaintCurve MyPaintInput::myPaintCurve() const
{
	return {m_box->isChecked(), m_xMax, m_xMin, m_yMax, m_yMin, getCurve()};
}

void MyPaintInput::setMyPaintCurve(const brushes::MyPaintCurve &curve)
{
	Q_ASSERT(curve.isValid());
	bool curveChanged = getCurve().points() != curve.curve.points();
	bool changed = curveChanged || m_xMax != curve.xMax ||
				   m_xMin != curve.xMin || m_yMax != curve.yMax ||
				   m_yMin != curve.yMin || m_box->isChecked() != curve.visible;
	if(changed) {
		m_xMax = qMin(curve.xMax, m_xHardMax);
		m_xMin = qMax(curve.xMin, m_xHardMin);
		m_yMax = qMin(curve.yMax, m_yHardMax);
		m_yMin = qMax(curve.yMin, m_yHardMin);
		if(curveChanged) {
			if(!m_curve) {
				constructCurveWidgets();
			}
			m_curve->setCurve(curve.curve);
		}
		m_box->setChecked(curve.visible);
		setCurveVisible(curve.visible);
	}
}

void MyPaintInput::changeBoxState(int state)
{
	setCurveVisible(state != Qt::Unchecked);
	emit controlPointsChanged();
}

void MyPaintInput::constructCurveWidgets()
{
	m_curveWrapper = new QWidget{this};
	layout()->addWidget(m_curveWrapper);
	QGridLayout *wrapperLayout = new QGridLayout;
	m_curveWrapper->setLayout(wrapperLayout);
	wrapperLayout->setContentsMargins(0, 0, 0, 0);

	m_ySpinner = new KisDoubleSliderSpinBox{this};
	wrapperLayout->addWidget(m_ySpinner, 0, 0, 1, 3);
	m_ySpinner->setPrefix(tr("Output Range: "));
	m_ySpinner->setRange(0.01, m_yHardMax, 2);
	connect(
		m_ySpinner,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		[this](double value) {
			m_yMax = value;
			m_yMin = -value;
			updateRanges();
			emit controlPointsChanged();
		});

	m_xMinSpinner = new KisDoubleSliderSpinBox{this};
	wrapperLayout->addWidget(m_xMinSpinner, 1, 0, 1, 3);
	m_xMinSpinner->setPrefix(tr("Input Minimum: "));
	m_xMinSpinner->setRange(m_yHardMin, m_yHardMax - 0.01, 2);
	connect(
		m_xMinSpinner,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		[this](double value) {
			m_xMin = value;
			if(m_xMax - m_xMin < 0.01) {
				m_xMax = m_xMin + 0.01;
			}
			updateRanges();
			emit controlPointsChanged();
		});

	m_xMaxSpinner = new KisDoubleSliderSpinBox{this};
	wrapperLayout->addWidget(m_xMaxSpinner, 2, 0, 1, 3);
	m_xMaxSpinner->setPrefix(tr("Input Maximum: "));
	m_xMaxSpinner->setRange(m_yHardMin + 0.01, m_yHardMax, 2);
	connect(
		m_xMaxSpinner,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		[this](double value) {
			m_xMax = value;
			if(m_xMax - m_xMin < 0.01) {
				m_xMin = m_xMax - 0.01;
			}
			updateRanges();
			emit controlPointsChanged();
		});

	m_yMaxLabel = new QLabel{this};
	wrapperLayout->addWidget(m_yMaxLabel, 3, 0, Qt::AlignRight | Qt::AlignTop);
	m_yMinLabel = new QLabel{this};
	wrapperLayout->addWidget(
		m_yMinLabel, 4, 0, Qt::AlignRight | Qt::AlignBottom);

	m_curve = new KisCurveWidget{this};
	wrapperLayout->addWidget(m_curve, 3, 1, 2, 2);
	m_curve->setMinimumSize(300, 300);
	m_curve->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_curve->setLinear(true);
	m_curve->setCurve(defaultCurve());

	m_xMaxLabel = new QLabel{this};
	wrapperLayout->addWidget(m_xMaxLabel, 5, 2, Qt::AlignRight | Qt::AlignTop);
	m_xMinLabel = new QLabel{this};
	wrapperLayout->addWidget(m_xMinLabel, 5, 1, Qt::AlignLeft | Qt::AlignTop);

	wrapperLayout->setColumnStretch(0, 0);
	wrapperLayout->setColumnStretch(1, 1);
	wrapperLayout->setColumnStretch(2, 1);

	connect(
		m_curve, &KisCurveWidget::curveChanged, this,
		&MyPaintInput::controlPointsChanged);
}

KisCubicCurve MyPaintInput::getCurve() const
{
	return m_curve ? m_curve->curve() : defaultCurve();
}

KisCubicCurve MyPaintInput::defaultCurve()
{
	return KisCubicCurve{{QPointF{0.0, 0.25}, QPointF{1.0, 0.75}}};
}

QPointF MyPaintInput::curveToControlPoint(const QPointF &point) const
{
	double x = translateCoordinate(point.x(), 0.0, 1.0, m_xMin, m_xMax);
	double y = translateCoordinate(point.y(), 0.0, 1.0, m_yMin, m_yMax);
	return QPointF{
		qBound(m_xHardMin, x, m_xHardMax), qBound(m_yHardMin, y, m_yHardMax)};
}

QPointF MyPaintInput::controlPointToCurve(
	const brushes::MyPaintCurve &curve, const QPointF &point)
{
	double x = translateCoordinate(point.x(), curve.xMin, curve.xMax, 0.0, 1.0);
	double y = translateCoordinate(point.y(), curve.yMin, curve.yMax, 0.0, 1.0);
	return QPointF{qBound(0.0, x, 1.0), qBound(0.0, y, 1.0)};
}

double MyPaintInput::translateCoordinate(
	double srcValue, double srcMin, double srcMax, double dstMin, double dstMax)
{
	return (dstMax - dstMin) / (srcMax - srcMin) * (srcValue - srcMin) + dstMin;
}

bool MyPaintInput::isNullControlPoints(const DP_MyPaintControlPoints &cps)
{
	if(cps.n >= 2) {
		for(int i = 0; i < cps.n; ++i) {
			if(cps.yvalues[i] != 0.0) {
				return false;
			}
		}
	}
	return true;
}

void MyPaintInput::setCurveVisible(bool visible)
{
	if(visible) {
		if(!m_curveWrapper) {
			constructCurveWidgets();
		}
		m_curveWrapper->setVisible(true);
		updateRanges();
	} else if(m_curveWrapper) {
		m_curveWrapper->setVisible(false);
	}
}

void MyPaintInput::updateRanges()
{
	if(m_curveWrapper) {
		m_xMinLabel->setText(QString::number(m_xMin, 'f', 2));
		m_xMaxLabel->setText(QString::number(m_xMax, 'f', 2));
		m_yMinLabel->setText(QString::number(m_yMin, 'f', 2));
		m_yMaxLabel->setText(QString::number(m_yMax, 'f', 2));
		m_ySpinner->setValue(m_yMax);
		m_xMinSpinner->setValue(m_xMin);
		m_xMaxSpinner->setValue(m_xMax);
	}
}

}
