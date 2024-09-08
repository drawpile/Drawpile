// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_BRUSHES_BRUSH_H
#define LIBCLIENT_BRUSHES_BRUSH_H
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

enum StabilizationMode {
	Stabilizer,
	Smoothing,
	LastStabilizationMode = Smoothing,
};

//! A convenience wrapper for classic brush settings
class ClassicBrush final : public DP_ClassicBrush {
public:
	ClassicBrush();

	bool equalPreset(const ClassicBrush &other, bool inEraserSlot) const;

	const KisCubicCurve &sizeCurve() const { return m_sizeCurve; }
	void setSizeCurve(const KisCubicCurve &sizeCurve);

	const KisCubicCurve &opacityCurve() const { return m_opacityCurve; }
	void setOpacityCurve(const KisCubicCurve &opacityCurve);

	const KisCubicCurve &hardnessCurve() const { return m_hardnessCurve; }
	void setHardnessCurve(const KisCubicCurve &hardnessCurve);

	const KisCubicCurve &smudgeCurve() const { return m_smudgeCurve; }
	void setSmudgeCurve(const KisCubicCurve &smudgeCurve);

	DP_ClassicBrushDynamicType lastSizeDynamicType() const
	{
		return m_lastSizeDynamicType;
	}

	void setSizeDynamicType(DP_ClassicBrushDynamicType type)
	{
		setDynamicType(type, size_dynamic.type, m_lastSizeDynamicType);
	}

	void setSizeMaxVelocity(float maxVelocity)
	{
		size_dynamic.max_velocity = maxVelocity;
	}

	void setSizeMaxDistance(float maxDistance)
	{
		size_dynamic.max_distance = maxDistance;
	}

	DP_ClassicBrushDynamicType lastOpacityDynamicType() const
	{
		return m_lastOpacityDynamicType;
	}

	void setOpacityDynamicType(DP_ClassicBrushDynamicType type)
	{
		setDynamicType(type, opacity_dynamic.type, m_lastOpacityDynamicType);
	}

	void setOpacityMaxVelocity(float maxVelocity)
	{
		opacity_dynamic.max_velocity = maxVelocity;
	}

	void setOpacityMaxDistance(float maxDistance)
	{
		opacity_dynamic.max_distance = maxDistance;
	}

	DP_ClassicBrushDynamicType lastHardnessDynamicType() const
	{
		return m_lastHardnessDynamicType;
	}

	void setHardnessDynamicType(DP_ClassicBrushDynamicType type)
	{
		setDynamicType(type, hardness_dynamic.type, m_lastHardnessDynamicType);
	}

	void setHardnessMaxVelocity(float maxVelocity)
	{
		hardness_dynamic.max_velocity = maxVelocity;
	}

	void setHardnessMaxDistance(float maxDistance)
	{
		hardness_dynamic.max_distance = maxDistance;
	}

	DP_ClassicBrushDynamicType lastSmudgeDynamicType() const
	{
		return m_lastSmudgeDynamicType;
	}

	void setSmudgeDynamicType(DP_ClassicBrushDynamicType type)
	{
		setDynamicType(type, smudge_dynamic.type, m_lastSmudgeDynamicType);
	}

	void setSmudgeMaxVelocity(float maxVelocity)
	{
		smudge_dynamic.max_velocity = maxVelocity;
	}

	void setSmudgeMaxDistance(float maxDistance)
	{
		smudge_dynamic.max_distance = maxDistance;
	}

	void setQColor(const QColor &c);
	QColor qColor() const;

	QJsonObject toJson() const;
	void exportToJson(QJsonObject &json) const;
	static ClassicBrush fromJson(const QJsonObject &json);
	bool fromExportJson(const QJsonObject &json);

	QPixmap presetThumbnail() const;

	StabilizationMode stabilizationMode;
	int stabilizerSampleCount;
	int smoothing;

private:
	static constexpr float DEFAULT_VELOCITY = 5.0f;
	static constexpr float DEFAULT_DISTANCE = 250.0f;

	void updateCurve(const KisCubicCurve &src, DP_ClassicBrushCurve &dst);
	static void setDynamicType(
		DP_ClassicBrushDynamicType type, DP_ClassicBrushDynamicType &outType,
		DP_ClassicBrushDynamicType &outLastType);

	void loadSettingsFromJson(const QJsonObject &settings);
	QJsonObject settingsToJson() const;

	static DP_ClassicBrushDynamic
	dynamicFromJson(const QJsonObject &o, const QString &prefix);
	static DP_ClassicBrushDynamicType
	lastDynamicTypeFromJson(const QJsonObject &o, const QString &prefix);
	static void dynamicToJson(
		const DP_ClassicBrushDynamic &dynamic,
		DP_ClassicBrushDynamicType lastType, const QString &prefix,
		QJsonObject &o);

	KisCubicCurve m_sizeCurve;
	KisCubicCurve m_opacityCurve;
	KisCubicCurve m_hardnessCurve;
	KisCubicCurve m_smudgeCurve;
	DP_ClassicBrushDynamicType m_lastSizeDynamicType =
		DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE;
	DP_ClassicBrushDynamicType m_lastOpacityDynamicType =
		DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE;
	DP_ClassicBrushDynamicType m_lastHardnessDynamicType =
		DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE;
	DP_ClassicBrushDynamicType m_lastSmudgeDynamicType =
		DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE;
};

struct MyPaintCurve final {
	bool visible = false;
	double xMax = std::numeric_limits<double>::lowest();
	double xMin = std::numeric_limits<double>::max();
	double yMax = std::numeric_limits<double>::lowest();
	double yMin = std::numeric_limits<double>::max();
	KisCubicCurve curve;

	bool isValid() const { return xMin <= xMax && yMin <= yMax; }

	static MyPaintCurve fromControlPoints(
		const DP_MyPaintControlPoints &cps,
		double xMax = std::numeric_limits<double>::lowest(),
		double xMin = std::numeric_limits<double>::max(),
		double yMax = std::numeric_limits<double>::lowest(),
		double yMin = std::numeric_limits<double>::max());

	static bool isNullControlPoints(const DP_MyPaintControlPoints &cps);

	QPointF controlPointToCurve(const QPointF &point);

	static double translateCoordinate(
		double srcValue, double srcMin, double srcMax, double dstMin,
		double dstMax);
};

class MyPaintBrush final {
public:
	MyPaintBrush();
	~MyPaintBrush();

	MyPaintBrush(const MyPaintBrush &other);
	MyPaintBrush(MyPaintBrush &&other);

	MyPaintBrush &operator=(MyPaintBrush &&other);
	MyPaintBrush &operator=(const MyPaintBrush &other);

	bool equalPreset(const MyPaintBrush &other, bool inEraserSlot) const;

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
	void setSmoothing(int smoothing) { m_smoothing = smoothing; }

	MyPaintCurve getCurve(int setting, int input) const;
	void setCurve(int setting, int input, const MyPaintCurve &curve);
	void removeCurve(int setting, int input);

	void setQColor(const QColor &c);
	QColor qColor() const;

	QJsonObject toJson() const;
	void exportToJson(QJsonObject &json) const;
	static MyPaintBrush fromJson(const QJsonObject &json);
	bool fromExportJson(const QJsonObject &json);

	QPixmap presetThumbnail() const;

private:
	DP_MyPaintBrush m_brush;
	DP_MyPaintSettings *m_settings;
	StabilizationMode m_stabilizationMode;
	int m_stabilizerSampleCount;
	int m_smoothing;
	QHash<QPair<int, int>, MyPaintCurve> m_curves;

	static const DP_MyPaintSettings &getDefaultSettings();

	QJsonObject mappingToJson() const;

	bool loadJsonSettings(const QJsonObject &o);
	bool loadJsonMapping(
		const QString &mappingKey, int settingId, float min, float max,
		const QJsonObject &o);
	bool loadJsonInputs(
		int settingId, const QJsonObject &o, const QJsonObject &ranges);
	void loadJsonRanges(
		int settingId, int inputId, const QString &inputKey,
		const DP_MyPaintControlPoints &cps, const QJsonObject &ranges);
};

class ActiveBrush final {
public:
	enum ActiveType { CLASSIC, MYPAINT };

	ActiveBrush(ActiveType activeType = CLASSIC);
	~ActiveBrush() = default;

	ActiveBrush(const ActiveBrush &other) = default;
	ActiveBrush(ActiveBrush &&other) = default;
	ActiveBrush &operator=(const ActiveBrush &other) = default;

	bool equalPreset(const ActiveBrush &other, bool inEraserSlot) const;

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
	void setEraser(bool erase);

	bool isEraserOverride() const { return m_eraserOverride; }
	void setEraserOverride(bool eraserOverride)
	{
		m_eraserOverride = eraserOverride;
	}

	DP_UPixelFloat color() const;
	QColor qColor() const;
	void setQColor(const QColor &c);

	StabilizationMode stabilizationMode() const;
	void setStabilizationMode(StabilizationMode stabilizationMode);

	int stabilizerSampleCount() const;
	void setStabilizerSampleCount(int stabilizerSampleCount);

	int smoothing() const;
	void setSmoothing(int smoothing);

	QByteArray toJson(bool includeSlotProperties = false) const;
	QByteArray toExportJson(const QString &description) const;
	QJsonObject toShareJson() const;
	static ActiveBrush
	fromJson(const QJsonObject &json, bool includeSlotProperties = false);
	bool fromExportJson(const QJsonObject &json);

	QString presetType() const;
	QByteArray presetData() const;
	QPixmap presetThumbnail() const;

	void setInBrushEngine(
		drawdance::BrushEngine &be, const DP_StrokeParams &stroke) const;

	void renderPreview(
		drawdance::BrushPreview &bp, DP_BrushPreviewShape shape) const;

private:
	ActiveType m_activeType;
	ClassicBrush m_classic;
	MyPaintBrush m_myPaint;
	bool m_eraserOverride = false;
};

}

Q_DECLARE_METATYPE(brushes::ClassicBrush)
Q_DECLARE_TYPEINFO(brushes::ClassicBrush, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(brushes::MyPaintBrush)
Q_DECLARE_METATYPE(brushes::ActiveBrush)

#endif
