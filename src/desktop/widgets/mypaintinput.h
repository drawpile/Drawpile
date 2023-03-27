// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MYPAINTINPUT_H
#define MYPAINTINPUT_H
#include "libclient/brushes/brush.h"
#include <QWidget>
#include <dpengine/libmypaint/mypaint-brush-settings.h>

class KisDoubleSliderSpinBox;
class QCheckBox;
class QLabel;

namespace widgets {

class CurveWidget;

class MyPaintInput final : public QWidget {
	Q_OBJECT
public:
	MyPaintInput(
		const QString &inputTitle, const QString &inputDescription,
		const QString &settingTitle, const MyPaintBrushSettingInfo *settingInfo,
		const MyPaintBrushInputInfo *inputInfo, QWidget *parent = nullptr);

	~MyPaintInput() override;

	DP_MyPaintControlPoints controlPoints() const;
	void setControlPoints(const DP_MyPaintControlPoints &cps);

	brushes::MyPaintCurve myPaintCurve() const;
	void setMyPaintCurve(const brushes::MyPaintCurve &curve);

signals:
	void controlPointsChanged();

private slots:
	void changeBoxState(int state);

private:
	void constructCurveWidgets();

	KisCubicCurve getCurve() const;
	static KisCubicCurve defaultCurve();

	QPointF curveToControlPoint(const QPointF &point) const;

	static QPointF controlPointToCurve(
		const brushes::MyPaintCurve &curve, const QPointF &point);

	static double translateCoordinate(
		double srcValue, double srcMin, double srcMax, double dstMin,
		double dstMax);

	static bool isNullControlPoints(const DP_MyPaintControlPoints &cps);

	void setCurveVisible(bool visible);
	void updateRanges();

	const QString m_inputTitle;
	const QString m_settingTitle;
	double m_xHardMax;
	double m_xHardMin;
	double m_yHardMax;
	double m_yHardMin;
	double m_xSoftMax;
	double m_xSoftMin;
	double m_ySoftMax;
	double m_ySoftMin;
	double m_xMax;
	double m_xMin;
	double m_yMax;
	double m_yMin;
	QCheckBox *m_box;
	QWidget *m_curveWrapper;
	KisDoubleSliderSpinBox *m_ySpinner;
	KisDoubleSliderSpinBox *m_xMinSpinner;
	KisDoubleSliderSpinBox *m_xMaxSpinner;
	CurveWidget *m_curve;
};

}

#endif
