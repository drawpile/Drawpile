// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/curve.h>
}
#include "libclient/utils/kis_cubic_curve.h"
#include <QList>
#include <QRandomGenerator>
#include <QVector>
#include <QtTest/QtTest>

class TestCurve final : public QObject {
	Q_OBJECT
private slots:
	void testCurves()
	{
		QRandomGenerator *rng = QRandomGenerator::global();
		for(int count = 0; count < 100; ++count) {
			QList<QPointF> points;

			for(int i = 0; i < count; ++i) {
				double x = qBound(0.0, rng->generateDouble() * 1.1, 1.0);
				double y = qBound(0.0, rng->generateDouble() * 1.1, 1.0);
				points.append({x, y});
			}

			KisCubicCurve kisCubicCurve(points);
			QCOMPARE(int(kisCubicCurve.points().size()), count);

			DP_Curve *dpCurve = KisCubicCurve::makeCurve(points);
			QVERIFY(dpCurve);
			QCOMPARE(DP_curve_type(dpCurve), DP_CURVE_TYPE_CUBIC);
			QCOMPARE(DP_curve_points_count(dpCurve), count);

			for(int i = 0; i < 1000; ++i) {
				double x = (rng->generateDouble() * 1.2) - 0.1;
				if(count == 0) {
					QCOMPARE(
						DP_curve_value_at(dpCurve, x), qBound(0.0, x, 1.0));
				} else if(count == 1) {
					QCOMPARE(DP_curve_value_at(dpCurve, x), points[0].y());
				} else {
					QCOMPARE(
						DP_curve_value_at(dpCurve, x), kisCubicCurve.value(x));
				}
			}

			DP_curve_decref(dpCurve);
		}
	}
};


QTEST_MAIN(TestCurve)
#include "curve.moc"
