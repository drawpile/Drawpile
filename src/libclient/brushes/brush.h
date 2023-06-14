// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_BRUSHES_BRUSH_H
#define DP_BRUSHES_BRUSH_H

extern "C" {
#include <dpengine/brush.h>
#include <dpengine/pixels.h>
}

#include "libclient/drawdance/brushpreview.h"
#include "libclient/utils/kis_cubic_curve.h"

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QMetaType>
#include <QPair>
#include <QPixmap>
#include <limits>

struct DP_StrokeParams;
struct MyPaintBrush;

namespace drawdance {
	class BrushEngine;
}

namespace brushes {

enum StabilizationMode { Stabilizer, Smoothing };

//! A convenience wrapper for classic brush settings
class ClassicBrush final : public DP_ClassicBrush
{
public:
	ClassicBrush();

	const KisCubicCurve &sizeCurve() const { return m_sizeCurve; }
	void setSizeCurve(const KisCubicCurve &sizeCurve);

	const KisCubicCurve &opacityCurve() const { return m_opacityCurve; }
	void setOpacityCurve(const KisCubicCurve &opacityCurve);

	const KisCubicCurve &hardnessCurve() const { return m_hardnessCurve; }
	void setHardnessCurve(const KisCubicCurve &hardnessCurve);

	const KisCubicCurve &smudgeCurve() const { return m_smudgeCurve; }
	void setSmudgeCurve(const KisCubicCurve &smudgeCurve);

	void setQColor(const QColor& c);
	QColor qColor() const;

	QJsonObject toJson() const;
	static ClassicBrush fromJson(const QJsonObject &json);

	QPixmap presetThumbnail() const;

	StabilizationMode stabilizationMode;
	int stabilizerSampleCount;
	int smoothing;

private:
	void updateCurve(const KisCubicCurve &src, DP_ClassicBrushCurve &dst);

	KisCubicCurve m_sizeCurve;
	KisCubicCurve m_opacityCurve;
	KisCubicCurve m_hardnessCurve;
	KisCubicCurve m_smudgeCurve;
};

struct MyPaintCurve final
{
	bool visible = false;
	double xMax = std::numeric_limits<double>::lowest();
	double xMin = std::numeric_limits<double>::max();
	double yMax = std::numeric_limits<double>::lowest();
	double yMin = std::numeric_limits<double>::max();
	KisCubicCurve curve;

	bool isValid() const
	{
		return xMin <= xMax && yMin <= yMax;
	}
};

class MyPaintBrush final
{
public:
	MyPaintBrush();
	~MyPaintBrush();

    MyPaintBrush(const MyPaintBrush &other);
	MyPaintBrush(MyPaintBrush &&other);

    MyPaintBrush &operator=(MyPaintBrush &&other);
    MyPaintBrush &operator=(const MyPaintBrush &other);

	DP_MyPaintBrush &brush() { return m_brush; }
	const DP_MyPaintBrush &constBrush() const { return m_brush; }

	DP_MyPaintSettings &settings();
	const DP_MyPaintSettings &constSettings() const;

	StabilizationMode stabilizationMode() const { return m_stabilizationMode; }
	void setStabilizationMode(StabilizationMode stabilizationMode)
	{
		m_stabilizationMode = stabilizationMode;
	}

	int stabilizerSampleCount() const { return m_stabilizerSampleCount; }
	void setStabilizerSampleCount(int stabilizerSampleCount)
	{
		m_stabilizerSampleCount = stabilizerSampleCount;
	}

	int smoothing() const { return m_smoothing; }
	void setSmoothing(int smoothing)
	{
		m_smoothing = smoothing;
	}

	MyPaintCurve getCurve(int setting, int input) const;
	void setCurve(int setting, int input, const MyPaintCurve &curve);
	void removeCurve(int setting, int input);

	void setQColor(const QColor& c);
	QColor qColor() const;

	QJsonObject toJson() const;
	static MyPaintBrush fromJson(const QJsonObject &json);

	bool loadMyPaintJson(const QJsonObject &json);

	QPixmap presetThumbnail() const;

private:
	DP_MyPaintBrush m_brush;
	DP_MyPaintSettings *m_settings;
	StabilizationMode m_stabilizationMode;
	int m_stabilizerSampleCount;
	int m_smoothing;
	QHash<QPair<int, int>, MyPaintCurve> m_curves;

	static const DP_MyPaintSettings &getDefaultSettings();

	bool loadJsonSettings(const QJsonObject &o);
	bool loadJsonMapping(const QString &mappingKey, int settingId, const QJsonObject &o);
	bool loadJsonInputs(const QString &mappingKey, int settingId, const QJsonObject &o);
};

class ActiveBrush final
{
public:
	enum ActiveType { CLASSIC, MYPAINT };

	ActiveBrush(ActiveType activeType = CLASSIC);
	~ActiveBrush() = default;

	ActiveBrush(const ActiveBrush &other) = default;
	ActiveBrush(ActiveBrush &&other) = default;
	ActiveBrush& operator=(const ActiveBrush &other) = default;

	ActiveType activeType() const { return m_activeType; }
	void setActiveType(ActiveType activeType) { m_activeType = activeType; }

	DP_BrushShape shape() const;

	ClassicBrush &classic() { return m_classic; }
	const ClassicBrush &classic() const { return m_classic; }
	void setClassic(const ClassicBrush &classic) { m_classic = classic; }

	MyPaintBrush &myPaint() { return m_myPaint; }
	const MyPaintBrush &myPaint() const { return m_myPaint; }
	void setMyPaint(const MyPaintBrush &myPaint) { m_myPaint = myPaint; }

	bool isEraser() const;

	DP_UPixelFloat color() const;
	QColor qColor() const;
	void setQColor(const QColor &c);

	StabilizationMode stabilizationMode() const;
	void setStabilizationMode(StabilizationMode stabilizationMode);

	int stabilizerSampleCount() const;
	void setStabilizerSampleCount(int stabilizerSampleCount);

	int smoothing() const;
	void setSmoothing(int smoothing);

	QJsonObject toJson() const;
	static ActiveBrush fromJson(const QJsonObject &json);

	QString presetType() const;
	QByteArray presetData() const;
	QPixmap presetThumbnail() const;

	void setInBrushEngine(
		drawdance::BrushEngine &be, const DP_StrokeParams &stroke) const;

	void renderPreview(drawdance::BrushPreview &bp, DP_BrushPreviewShape shape) const;

private:
	ActiveType m_activeType;
	ClassicBrush m_classic;
	MyPaintBrush m_myPaint;
};

}

Q_DECLARE_METATYPE(brushes::ClassicBrush)
Q_DECLARE_TYPEINFO(brushes::ClassicBrush, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(brushes::MyPaintBrush)
Q_DECLARE_METATYPE(brushes::ActiveBrush)

#endif


