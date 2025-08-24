// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/mypaintinput.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/brushes/brush.h"
#include <QCheckBox>
#include <QLabel>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QtGlobal>
#include <QtMath>

namespace widgets {

MyPaintInput::MyPaintInput(
	const QString &inputTitle, const QString &inputDescription,
	const QString &settingTitle, const MyPaintBrushSettingInfo *settingInfo,
	const MyPaintBrushInputInfo *inputInfo, QWidget *parent)
	: QWidget{parent}
	, m_inputTitle{inputTitle}
	, m_settingTitle{settingTitle}
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

	m_box = new QCheckBox{inputTitle, this};
	widgetLayout->addWidget(m_box);
	m_box->setToolTip(inputDescription);
	connect(
		m_box, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&MyPaintInput::changeBoxState, Qt::QueuedConnection);

	// Constructing these widgets in large volumes is slow and make the brush
	// setting dialog take a long time to appear, so we lazily initialize them.
	m_curveWrapper = nullptr;
	m_ySpinner = nullptr;
	m_xMinSpinner = nullptr;
	m_xMaxSpinner = nullptr;
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
	brushes::MyPaintCurve curve = brushes::MyPaintCurve::fromControlPoints(cps);
	if(!curve.visible) {
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

void MyPaintInput::changeBoxState(compat::CheckBoxState state)
{
	setCurveVisible(state != Qt::Unchecked);
	emit controlPointsChanged();
}

void MyPaintInput::constructCurveWidgets()
{
	m_curveWrapper = new QWidget{this};
	layout()->addWidget(m_curveWrapper);
	QVBoxLayout *wrapperLayout = new QVBoxLayout;
	m_curveWrapper->setLayout(wrapperLayout);
	wrapperLayout->setContentsMargins(0, 0, 0, 0);

	m_ySpinner = new KisDoubleSliderSpinBox{this};
	wrapperLayout->addWidget(m_ySpinner);
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
	wrapperLayout->addWidget(m_xMinSpinner);
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
	wrapperLayout->addWidget(m_xMaxSpinner);
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

	m_curve = new CurveWidget{m_inputTitle, m_settingTitle, true, this};
	wrapperLayout->addWidget(m_curve);
	m_curve->setCurve(defaultCurve());
	// Keep the layout from jerking around as values change by adding an
	// invisible label with text as wide as the widest possible output label.
	m_curve->addVerticalSpacingLabel(
		QString::number(-qAbs(m_yHardMax), 'f', 2)
			.replace(QRegularExpression{"[0-8]"}, "9"));

	connect(
		m_curve, &CurveWidget::curveChanged, this,
		&MyPaintInput::controlPointsChanged);

	emit curveWidgetsConstructed();
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
	double x = brushes::MyPaintCurve::translateCoordinate(
		point.x(), 0.0, 1.0, m_xMin, m_xMax);
	double y = brushes::MyPaintCurve::translateCoordinate(
		point.y(), 0.0, 1.0, m_yMin, m_yMax);
	return QPointF{
		qBound(m_xHardMin, x, m_xHardMax), qBound(m_yHardMin, y, m_yHardMax)};
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
		m_curve->setAxisValueLabels(
			QString::number(m_xMin, 'f', 2), QString::number(m_xMax, 'f', 2),
			QString::number(m_yMin, 'f', 2), QString::number(m_yMax, 'f', 2));
		m_ySpinner->setValue(m_yMax);
		m_xMinSpinner->setValue(m_xMin);
		m_xMaxSpinner->setValue(m_xMax);
	}
}

}
