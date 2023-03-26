// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CURVEWIDGET_H
#define CURVEWIDGET_H

#include "libclient/utils/kis_cubic_curve.h"
#include <QWidget>

class KisCurveWidget;
class QAbstractButton;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace widgets {

class CurveWidget : public QWidget {
	Q_OBJECT
public:
	explicit CurveWidget(bool linear, QWidget *parent = nullptr);
	~CurveWidget();

	CurveWidget(const CurveWidget &) = delete;
	CurveWidget(CurveWidget &&) = delete;
	CurveWidget &operator=(const CurveWidget &) = delete;
	CurveWidget &operator=(CurveWidget &&) = delete;

	KisCubicCurve curve() const;
	void setCurve(const KisCubicCurve &curve);

	void addVerticalSpacingLabel(const QString &text);

	void addButton(QAbstractButton *button);

	void setAxisLabels(
		const QString &xMin, const QString &xMax, const QString &yMin,
		const QString &yMax);

signals:
	void curveChanged(const KisCubicCurve &curve);

private slots:
	void copyCurve();
	void pasteCurve();
	void saveCurve();
	void loadCurve();

private:
	KisCurveWidget *m_curve;
	QLabel *m_yMaxLabel;
	QLabel *m_yMinLabel;
	QLabel *m_xMaxLabel;
	QLabel *m_xMinLabel;
	QVBoxLayout *m_buttonLayout;
	QPushButton *m_copyButton;
	QPushButton *m_pasteButton;
	QPushButton *m_presetsButton;
};

}

#endif
