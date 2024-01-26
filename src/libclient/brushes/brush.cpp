// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/brushes/brush.h"
#include "cmake-config/config.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/drawdance/brushengine.h"
#include "libshared/util/qtcompat.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
#include <mypaint-brush.h>

namespace {

void setDrawdanceColorToQColor(DP_UPixelFloat &r, const QColor &q)
{
	r = {
		compat::cast<float>(q.blueF()), compat::cast<float>(q.greenF()),
		compat::cast<float>(q.redF()), compat::cast<float>(q.alphaF())};
}

QColor drawdanceColorToQColor(const DP_UPixelFloat &color)
{
	return QColor::fromRgbF(color.r, color.g, color.b, color.a);
}

}

namespace brushes {

ClassicBrush::ClassicBrush()
	: DP_ClassicBrush{
		{1.0f, 10.0f, {}},
		{0.0f, 1.0f, {}},
		{0.0f, 1.0f, {}},
		{0.0f, 0.0f, {}},
		0.1f,
		0,
		{0.0f, 0.0f, 0.0f, 1.0f},
		DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND,
		DP_BLEND_MODE_NORMAL,
		DP_BLEND_MODE_ERASE,
		false,
		true,
		false,
		{DP_CLASSIC_BRUSH_DYNAMIC_NONE, DEFAULT_VELOCITY, DEFAULT_DISTANCE},
		{DP_CLASSIC_BRUSH_DYNAMIC_NONE, DEFAULT_VELOCITY, DEFAULT_DISTANCE},
		{DP_CLASSIC_BRUSH_DYNAMIC_NONE, DEFAULT_VELOCITY, DEFAULT_DISTANCE},
		{DP_CLASSIC_BRUSH_DYNAMIC_NONE, DEFAULT_VELOCITY, DEFAULT_DISTANCE},
	}
	, stabilizationMode(Stabilizer)
	, stabilizerSampleCount(0)
	, smoothing(0)
{
	updateCurve(m_sizeCurve, size.curve);
	updateCurve(m_opacityCurve, opacity.curve);
	updateCurve(m_hardnessCurve, hardness.curve);
	updateCurve(m_smudgeCurve, smudge.curve);
}

void ClassicBrush::setSizeCurve(const KisCubicCurve &sizeCurve)
{
	m_sizeCurve = sizeCurve;
	updateCurve(m_sizeCurve, size.curve);
}

void ClassicBrush::setOpacityCurve(const KisCubicCurve &opacityCurve)
{
	m_opacityCurve = opacityCurve;
	updateCurve(m_opacityCurve, opacity.curve);
}

void ClassicBrush::setHardnessCurve(const KisCubicCurve &hardnessCurve)
{
	m_hardnessCurve = hardnessCurve;
	updateCurve(m_hardnessCurve, hardness.curve);
}

void ClassicBrush::setSmudgeCurve(const KisCubicCurve &smudgeCurve)
{
	m_smudgeCurve = smudgeCurve;
	updateCurve(m_smudgeCurve, smudge.curve);
}

void ClassicBrush::setQColor(const QColor &c)
{
	setDrawdanceColorToQColor(color, c);
}

QColor ClassicBrush::qColor() const
{
	return drawdanceColorToQColor(color);
}

QJsonObject ClassicBrush::toJson() const
{
	return QJsonObject{
		{"type", "dp-classic"},
		{"version", 1},
		{"settings", settingsToJson()},
	};
}

void ClassicBrush::exportToJson(QJsonObject &json) const
{
	json["drawpile_classic_settings"] = settingsToJson();
	json["settings"] = QJsonObject{};
}

ClassicBrush ClassicBrush::fromJson(const QJsonObject &json)
{
	ClassicBrush b;
	if(json["type"] != "dp-classic") {
		qWarning("ClassicBrush::fromJson: type is not dp-classic!");
		return b;
	}

	const QJsonObject o = json["settings"].toObject();

	if(o["shape"] == "round-pixel")
		b.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND;
	else if(o["shape"] == "square-pixel")
		b.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
	else
		b.shape = DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;

	b.size.max = o["size"].toDouble();
	b.size.min = o["size2"].toDouble();
	b.m_sizeCurve.fromString(o["sizecurve"].toString());
	b.updateCurve(b.m_sizeCurve, b.size.curve);

	b.opacity.max = o["opacity"].toDouble();
	b.opacity.min = o["opacity2"].toDouble();
	b.m_opacityCurve.fromString(o["opacitycurve"].toString());
	b.updateCurve(b.m_opacityCurve, b.opacity.curve);

	b.hardness.max = o["hard"].toDouble();
	b.hardness.min = o["hard2"].toDouble();
	b.m_hardnessCurve.fromString(o["hardcurve"].toString());
	b.updateCurve(b.m_hardnessCurve, b.hardness.curve);

	b.smudge.max = o["smudge"].toDouble();
	b.smudge.min = o["smudge2"].toDouble();
	b.m_smudgeCurve.fromString(o["smudgecurve"].toString());
	b.updateCurve(b.m_smudgeCurve, b.smudge.curve);

	b.spacing = o["spacing"].toDouble();
	b.resmudge = o["resmudge"].toInt();

	b.incremental = !o["indirect"].toBool();
	b.colorpick = o["colorpick"].toBool();

	b.size_dynamic = dynamicFromJson(o, QStringLiteral("size"));
	b.m_lastSizeDynamicType =
		lastDynamicTypeFromJson(o, QStringLiteral("size"));
	b.hardness_dynamic = dynamicFromJson(o, QStringLiteral("hard"));
	b.m_lastHardnessDynamicType =
		lastDynamicTypeFromJson(o, QStringLiteral("hard"));
	b.opacity_dynamic = dynamicFromJson(o, QStringLiteral("opacity"));
	b.m_lastOpacityDynamicType =
		lastDynamicTypeFromJson(o, QStringLiteral("opacity"));
	b.smudge_dynamic = dynamicFromJson(o, QStringLiteral("smudge"));
	b.m_lastSmudgeDynamicType =
		lastDynamicTypeFromJson(o, QStringLiteral("smudge"));

	b.brush_mode = canvas::blendmode::fromSvgName(o["blend"].toString());
	b.erase_mode = canvas::blendmode::fromSvgName(
		o["blenderase"].toString(), DP_BLEND_MODE_ERASE);
	b.erase = o["erase"].toBool();

	b.stabilizationMode =
		o["stabilizationmode"].toInt() == Smoothing ? Smoothing : Stabilizer;
	b.stabilizerSampleCount = o["stabilizer"].toInt();
	b.smoothing = o["smoothing"].toInt();

	return b;
}

bool ClassicBrush::fromExportJson(const QJsonObject &json)
{
	QJsonObject settings = json["drawpile_classic_settings"].toObject();
	if(settings.isEmpty()) {
		return false;
	} else {
		loadSettingsFromJson(settings);
		return true;
	}
}

QPixmap ClassicBrush::presetThumbnail() const
{
	QColor c =
		smudge.max > 0.0f ? QColor::fromRgbF(0.1f, 0.6f, 0.9f) : Qt::gray;
	return drawdance::BrushPreview::classicBrushPreviewDab(*this, 64, 64, c);
}

void ClassicBrush::updateCurve(
	const KisCubicCurve &src, DP_ClassicBrushCurve &dst)
{
	const QVector<qreal> &values =
		src.floatTransfer(DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT);
	for(int i = 0; i < DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT; ++i) {
		dst.values[i] = values[i];
	}
}

void ClassicBrush::setDynamicType(
	DP_ClassicBrushDynamicType type, DP_ClassicBrushDynamicType &outType,
	DP_ClassicBrushDynamicType &outLastType)
{
	switch(type) {
	case DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE:
	case DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY:
	case DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE:
		outLastType = type;
		[[fallthrough]];
	case DP_CLASSIC_BRUSH_DYNAMIC_NONE:
		outType = type;
		return;
	}
	qWarning("Unhandled dynamic type %d", int(type));
}

void ClassicBrush::loadSettingsFromJson(const QJsonObject &settings)
{
	if(settings["shape"] == "round-pixel") {
		shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND;
	} else if(settings["shape"] == "square-pixel") {
		shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
	} else {
		shape = DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;
	}

	size.max = settings["size"].toDouble();
	size.min = settings["size2"].toDouble();
	m_sizeCurve.fromString(settings["sizecurve"].toString());
	updateCurve(m_sizeCurve, size.curve);

	opacity.max = settings["opacity"].toDouble();
	opacity.min = settings["opacity2"].toDouble();
	m_opacityCurve.fromString(settings["opacitycurve"].toString());
	updateCurve(m_opacityCurve, opacity.curve);

	hardness.max = settings["hard"].toDouble();
	hardness.min = settings["hard2"].toDouble();
	m_hardnessCurve.fromString(settings["hardcurve"].toString());
	updateCurve(m_hardnessCurve, hardness.curve);

	smudge.max = settings["smudge"].toDouble();
	smudge.min = settings["smudge2"].toDouble();
	m_smudgeCurve.fromString(settings["smudgecurve"].toString());
	updateCurve(m_smudgeCurve, smudge.curve);

	spacing = settings["spacing"].toDouble();
	resmudge = settings["resmudge"].toInt();

	incremental = !settings["indirect"].toBool();
	colorpick = settings["colorpick"].toBool();
	size_dynamic = dynamicFromJson(settings, QStringLiteral("size"));
	m_lastSizeDynamicType =
		lastDynamicTypeFromJson(settings, QStringLiteral("size"));
	hardness_dynamic = dynamicFromJson(settings, QStringLiteral("hard"));
	m_lastHardnessDynamicType =
		lastDynamicTypeFromJson(settings, QStringLiteral("hard"));
	opacity_dynamic = dynamicFromJson(settings, QStringLiteral("opacity"));
	m_lastOpacityDynamicType =
		lastDynamicTypeFromJson(settings, QStringLiteral("opacity"));
	smudge_dynamic = dynamicFromJson(settings, QStringLiteral("smudge"));
	m_lastSmudgeDynamicType =
		lastDynamicTypeFromJson(settings, QStringLiteral("smudge"));

	brush_mode = canvas::blendmode::fromSvgName(settings["blend"].toString());
	erase_mode = canvas::blendmode::fromSvgName(
		settings["blenderase"].toString(), DP_BLEND_MODE_ERASE);
	erase = settings["erase"].toBool();

	stabilizationMode = settings["stabilizationmode"].toInt() == Smoothing
							? Smoothing
							: Stabilizer;
	stabilizerSampleCount = settings["stabilizer"].toInt();
	smoothing = settings["smoothing"].toInt();
}

QJsonObject ClassicBrush::settingsToJson() const
{
	QJsonObject o;
	switch(shape) {
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
		o["shape"] = "round-pixel";
		break;
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
		o["shape"] = "square-pixel";
		break;
	default:
		o["shape"] = "round-soft";
		break;
	}

	o["size"] = size.max;
	if(size.min > 0)
		o["size2"] = size.min;
	o["sizecurve"] = m_sizeCurve.toString();

	o["opacity"] = opacity.max;
	if(opacity.min > 0)
		o["opacity2"] = opacity.min;
	o["opacitycurve"] = m_opacityCurve.toString();

	o["hard"] = hardness.max;
	if(hardness.min > 0)
		o["hard2"] = hardness.min;
	o["hardcurve"] = m_hardnessCurve.toString();

	if(smudge.max > 0)
		o["smudge"] = smudge.max;
	if(smudge.min > 0)
		o["smudgeratio"] = smudge.min;
	o["smudgecurve"] = m_smudgeCurve.toString();

	o["spacing"] = spacing;
	if(resmudge > 0)
		o["resmudge"] = resmudge;

	if(!incremental)
		o["indirect"] = true;
	if(colorpick)
		o["colorpick"] = true;
	dynamicToJson(
		size_dynamic, m_lastSizeDynamicType, QStringLiteral("size"), o);
	dynamicToJson(
		hardness_dynamic, m_lastHardnessDynamicType, QStringLiteral("hard"), o);
	dynamicToJson(
		opacity_dynamic, m_lastOpacityDynamicType, QStringLiteral("opacity"),
		o);
	dynamicToJson(
		smudge_dynamic, m_lastSmudgeDynamicType, QStringLiteral("smudge"), o);

	o["blend"] = canvas::blendmode::svgName(brush_mode);
	o["blenderase"] = canvas::blendmode::svgName(erase_mode);
	o["erase"] = erase;

	o["stabilizationmode"] = stabilizationMode;
	o["stabilizer"] = stabilizerSampleCount;
	o["smoothing"] = smoothing;

	// Note: color is intentionally omitted

	return o;
}

DP_ClassicBrushDynamic
ClassicBrush::dynamicFromJson(const QJsonObject &o, const QString &prefix)
{
	DP_ClassicBrushDynamicType type;
	if(o[prefix + QStringLiteral("p")].toBool()) {
		type = DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE;
	} else if(o[prefix + QStringLiteral("v")].toBool()) {
		type = DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY;
	} else if(o[prefix + QStringLiteral("d")].toBool()) {
		type = DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE;
	} else {
		type = DP_CLASSIC_BRUSH_DYNAMIC_NONE;
	}
	float maxVelocity = qMax(
		float(o[prefix + QStringLiteral("vmax")].toDouble(DEFAULT_VELOCITY)),
		0.0f);
	float maxDistance = qMax(
		float(o[prefix + QStringLiteral("dmax")].toDouble(DEFAULT_DISTANCE)),
		0.0f);
	return {type, maxVelocity, maxDistance};
}

DP_ClassicBrushDynamicType ClassicBrush::lastDynamicTypeFromJson(
	const QJsonObject &o, const QString &prefix)
{
	if(o[prefix + QStringLiteral("lastv")].toBool()) {
		return DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY;
	} else if(o[prefix + QStringLiteral("lastd")].toBool()) {
		return DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE;
	} else {
		return DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE;
	}
}

void ClassicBrush::dynamicToJson(
	const DP_ClassicBrushDynamic &dynamic, DP_ClassicBrushDynamicType lastType,
	const QString &prefix, QJsonObject &o)
{
	switch(dynamic.type) {
	case DP_CLASSIC_BRUSH_DYNAMIC_NONE:
		break;
	case DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE:
		o[prefix + QStringLiteral("p")] = true;
		break;
	case DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY:
		o[prefix + QStringLiteral("v")] = true;
		break;
	case DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE:
		o[prefix + QStringLiteral("d")] = true;
		break;
	}
	switch(lastType) {
	case DP_CLASSIC_BRUSH_DYNAMIC_NONE:
	case DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE:
		break;
	case DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY:
		o[prefix + QStringLiteral("lastv")] = true;
		break;
	case DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE:
		o[prefix + QStringLiteral("lastd")] = true;
		break;
	}
	o[prefix + QStringLiteral("vmax")] = dynamic.max_velocity;
	o[prefix + QStringLiteral("dmax")] = dynamic.max_distance;
}


MyPaintBrush::MyPaintBrush()
	: m_brush{{0.0f, 0.0f, 0.0f, 1.0f}, false, false, true}
	, m_settings{nullptr}
	, m_stabilizationMode{Stabilizer}
	, m_stabilizerSampleCount{0}
	, m_smoothing{0}
	, m_curves{}
{
}

MyPaintBrush::~MyPaintBrush()
{
	delete m_settings;
}

MyPaintBrush::MyPaintBrush(const MyPaintBrush &other)
	: m_brush{other.m_brush}
	, m_settings{nullptr}
	, m_stabilizationMode{other.m_stabilizationMode}
	, m_stabilizerSampleCount{other.m_stabilizerSampleCount}
	, m_smoothing{other.m_smoothing}
	, m_curves{other.m_curves}
{
	if(other.m_settings) {
		m_settings = new DP_MyPaintSettings;
		*m_settings = *other.m_settings;
	}
}

MyPaintBrush::MyPaintBrush(MyPaintBrush &&other)
	: m_brush{other.m_brush}
	, m_settings{other.m_settings}
	, m_stabilizationMode{other.m_stabilizationMode}
	, m_stabilizerSampleCount{other.m_stabilizerSampleCount}
	, m_smoothing{other.m_smoothing}
	, m_curves{other.m_curves}
{
	other.m_settings = nullptr;
}

MyPaintBrush &MyPaintBrush::operator=(MyPaintBrush &&other)
{
	std::swap(m_brush, other.m_brush);
	std::swap(m_settings, other.m_settings);
	std::swap(m_stabilizationMode, other.m_stabilizationMode);
	std::swap(m_stabilizerSampleCount, other.m_stabilizerSampleCount);
	std::swap(m_smoothing, other.m_smoothing);
	std::swap(m_curves, other.m_curves);
	return *this;
}

MyPaintBrush &MyPaintBrush::operator=(const MyPaintBrush &other)
{
	m_brush = other.m_brush;
	if(other.m_settings) {
		if(!m_settings) {
			m_settings = new DP_MyPaintSettings;
		}
		*m_settings = *other.m_settings;
	} else {
		delete m_settings;
		m_settings = nullptr;
	}
	m_stabilizationMode = other.m_stabilizationMode;
	m_stabilizerSampleCount = other.m_stabilizerSampleCount;
	m_smoothing = other.m_smoothing;
	m_curves = other.m_curves;
	return *this;
}

DP_MyPaintSettings &MyPaintBrush::settings()
{
	if(!m_settings) {
		m_settings = new DP_MyPaintSettings(getDefaultSettings());
	}
	return *m_settings;
}

const DP_MyPaintSettings &MyPaintBrush::constSettings() const
{
	return m_settings ? *m_settings : getDefaultSettings();
}

MyPaintCurve MyPaintBrush::getCurve(int setting, int input) const
{
	return m_curves.value({setting, input});
}

void MyPaintBrush::setCurve(int setting, int input, const MyPaintCurve &curve)
{
	m_curves.insert({setting, input}, curve);
}

void MyPaintBrush::removeCurve(int setting, int input)
{
	m_curves.remove({setting, input});
}

void MyPaintBrush::setQColor(const QColor &c)
{
	setDrawdanceColorToQColor(m_brush.color, c);
}

QColor MyPaintBrush::qColor() const
{
	return drawdanceColorToQColor(m_brush.color);
}

QJsonObject MyPaintBrush::toJson() const
{
	return QJsonObject{
		{"type", "dp-mypaint"},
		{"version", 1},
		{"settings",
		 QJsonObject{
			 {"lock_alpha", m_brush.lock_alpha},
			 {"erase", m_brush.erase},
			 {"indirect", !m_brush.incremental},
			 {"stabilizationmode", m_stabilizationMode},
			 {"stabilizer", m_stabilizerSampleCount},
			 {"smoothing", m_smoothing},
			 {"mapping", mappingToJson()},
		 }},
	};
}

void MyPaintBrush::exportToJson(QJsonObject &json) const
{
	json["drawpile_settings"] = QJsonObject{
		{"lock_alpha", m_brush.lock_alpha},
		{"erase", m_brush.erase},
		{"indirect", !m_brush.incremental},
		{"stabilizationmode", m_stabilizationMode},
		{"stabilizer", m_stabilizerSampleCount},
		{"smoothing", m_smoothing},
	};
	json["settings"] = mappingToJson();
}

MyPaintBrush MyPaintBrush::fromJson(const QJsonObject &json)
{
	MyPaintBrush b;
	if(json["type"] != "dp-mypaint") {
		qWarning("MyPaintBrush::fromJson: type is not dp-mypaint!");
		return b;
	}

	const QJsonObject o = json["settings"].toObject();
	b.m_brush.lock_alpha = o["lock_alpha"].toBool();
	b.m_brush.erase = o["erase"].toBool();
	b.m_brush.incremental = !o["indirect"].toBool();
	b.loadJsonSettings(o["mapping"].toObject());

	// If there's no Drawpile stabilizer defined, we get a sensible default
	// value from the slow tracking setting, which is also a kind of stabilizer.
	int stabilizerSampleCount = o["stabilizer"].toInt(-1);
	if(stabilizerSampleCount < 0) {
		if(b.m_settings) {
			const DP_MyPaintMapping &slowTrackingSetting =
				b.m_settings->mappings[MYPAINT_BRUSH_SETTING_SLOW_TRACKING];
			stabilizerSampleCount =
				qRound(slowTrackingSetting.base_value * 10.0f);
		} else {
			stabilizerSampleCount = 0;
		}
	}
	b.m_stabilizationMode =
		o["stabilizationmode"].toInt() == Smoothing ? Smoothing : Stabilizer;
	b.m_stabilizerSampleCount = stabilizerSampleCount;
	b.m_smoothing = o["smoothing"].toInt();

	return b;
}

bool MyPaintBrush::fromExportJson(const QJsonObject &json)
{
	if(json["version"].toInt() != 3) {
		qWarning("MyPaintBrush::fromMyPaintJson: version not 3");
		return false;
	}

	if(!loadJsonSettings(json["settings"].toObject())) {
		return false;
	}

	QJsonObject drawpileSettings = json["drawpile_settings"].toObject();
	if(drawpileSettings.isEmpty()) {
		// Workaround for MyPaint exporting the current alpha lock state for
		// this value. MyPaint doesn't import this either.
		float &lockAlpha =
			m_settings->mappings[MYPAINT_BRUSH_SETTING_LOCK_ALPHA].base_value;
		if(lockAlpha == 1.0f) {
			lockAlpha = 0.0f;
		}
	}

	m_brush.lock_alpha = drawpileSettings["lock_alpha"].toBool(false);
	m_brush.erase = drawpileSettings["erase"].toBool(false);
	m_brush.incremental = !drawpileSettings["indirect"].toBool(false);
	m_stabilizationMode =
		drawpileSettings["stabilizationmode"].toInt() == Smoothing ? Smoothing
																   : Stabilizer;
	m_stabilizerSampleCount = drawpileSettings["stabilizer"].toInt(-1);
	m_smoothing = drawpileSettings["smoothing"].toInt();

	return true;
}

QPixmap MyPaintBrush::presetThumbnail() const
{
	// MyPaint brushes are too complicated to generate a single dab to represent
	// them, so just make a blank image and have the user pick one themselves.
	// At the time of writing, there's no MyPaint brush editor within Drawpile
	// anyway, so most brushes will presumably be imported, not generated anew.
	QPixmap pixmap(64, 64);
	pixmap.fill(Qt::white);
	return pixmap;
}

const DP_MyPaintSettings &MyPaintBrush::getDefaultSettings()
{
	static DP_MyPaintSettings settings;
	static bool settingsInitialized;
	if(!settingsInitialized) {
		settingsInitialized = true;
		// Same procedure as mypaint_brush_from_defaults.
		for(int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
			DP_MyPaintMapping &mapping = settings.mappings[i];
			mapping.base_value =
				mypaint_brush_setting_info(static_cast<MyPaintBrushSetting>(i))
					->def;
			for(int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
				mapping.inputs[j].n = 0;
			}
		}
		DP_MyPaintMapping &opaqueMultiplyMapping =
			settings.mappings[MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY];
		DP_MyPaintControlPoints &brushInputPressure =
			opaqueMultiplyMapping.inputs[MYPAINT_BRUSH_INPUT_PRESSURE];
		brushInputPressure.n = 2;
		brushInputPressure.xvalues[0] = 0.0;
		brushInputPressure.yvalues[0] = 0.0;
		brushInputPressure.xvalues[1] = 1.0;
		brushInputPressure.yvalues[1] = 1.0;
	}
	return settings;
}

QJsonObject MyPaintBrush::mappingToJson() const
{
	QJsonObject o;
	if(m_settings) {
		for(int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
			const DP_MyPaintMapping &mapping = m_settings->mappings[i];

			QJsonObject inputs;
			for(int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
				const DP_MyPaintControlPoints &cps = mapping.inputs[j];
				if(cps.n) {
					QJsonArray points;
					for(int k = 0; k < cps.n; ++k) {
						points.append(QJsonArray{
							cps.xvalues[k],
							cps.yvalues[k],
						});
					}
					MyPaintBrushInput inputId =
						static_cast<MyPaintBrushInput>(j);
					inputs[mypaint_brush_input_info(inputId)->cname] = points;
				}
			}

			MyPaintBrushSetting settingId = static_cast<MyPaintBrushSetting>(i);
			o[mypaint_brush_setting_info(settingId)->cname] = QJsonObject{
				{"base_value", mapping.base_value},
				{"inputs", inputs},
			};
		}
	}
	return o;
}

bool MyPaintBrush::loadJsonSettings(const QJsonObject &o)
{
	bool foundSetting = false;
	for(int settingId = 0; settingId < MYPAINT_BRUSH_SETTINGS_COUNT;
		++settingId) {
		const MyPaintBrushSettingInfo *info =
			mypaint_brush_setting_info(MyPaintBrushSetting(settingId));
		QString mappingKey = QString::fromUtf8(info->cname);
		bool loaded =
			o.contains(mappingKey) &&
			loadJsonMapping(mappingKey, settingId, o[mappingKey].toObject());
		if(loaded) {
			foundSetting = true;
		}
	}
	return foundSetting;
}

bool MyPaintBrush::loadJsonMapping(
	const QString &mappingKey, int settingId, const QJsonObject &o)
{
	bool foundSetting = false;

	if(o["base_value"].isDouble()) {
		settings().mappings[settingId].base_value = o["base_value"].toDouble();
		foundSetting = true;
	} else {
		qWarning("Bad MyPaint 'base_value' in %s", qPrintable(mappingKey));
	}

	if(!o["inputs"].isObject()) {
		qWarning("Bad MyPaint 'inputs' in %s", qPrintable(mappingKey));
	} else if(loadJsonInputs(mappingKey, settingId, o["inputs"].toObject())) {
		foundSetting = true;
	}

	return foundSetting;
}

bool MyPaintBrush::loadJsonInputs(int settingId, const QJsonObject &o)
{
	bool foundSetting = false;

	for(int inputId = 0; inputId < MYPAINT_BRUSH_INPUTS_COUNT; ++inputId) {
		const MyPaintBrushInputInfo *info =
			mypaint_brush_input_info(MyPaintBrushInput(inputId));
		QString inputKey = QString::fromUtf8(info->cname);
		QJsonValue value = o[inputKey];
		if(value.isArray()) {
			foundSetting = true;
			DP_MyPaintControlPoints &cps =
				settings().mappings[settingId].inputs[inputId];
			const QJsonArray points = o[inputKey].toArray();

			static constexpr int MAX_POINTS =
				sizeof(cps.xvalues) / sizeof(cps.xvalues[0]);
			cps.n = qMin(MAX_POINTS, points.count());

			for(int i = 0; i < cps.n; ++i) {
				const QJsonArray point = points[i].toArray();
				cps.xvalues[i] = point.at(0).toDouble();
				cps.yvalues[i] = point.at(1).toDouble();
			}
		} else {
			settings().mappings[settingId].inputs[inputId].n = 0;
		}
	}

	return foundSetting;
}


ActiveBrush::ActiveBrush(ActiveType activeType)
	: m_activeType(activeType)
	, m_classic()
	, m_myPaint()
{
}

DP_BrushShape ActiveBrush::shape() const
{
	return m_activeType == CLASSIC ? m_classic.shape : DP_BRUSH_SHAPE_MYPAINT;
}

bool ActiveBrush::isEraser() const
{
	return m_activeType == CLASSIC ? m_classic.erase
								   : m_myPaint.constBrush().erase;
}

DP_UPixelFloat ActiveBrush::color() const
{
	return m_activeType == CLASSIC ? m_classic.color
								   : m_myPaint.constBrush().color;
}

QColor ActiveBrush::qColor() const
{
	return m_activeType == CLASSIC ? m_classic.qColor() : m_myPaint.qColor();
}

void ActiveBrush::setQColor(const QColor &c)
{
	m_classic.setQColor(c);
	m_myPaint.setQColor(c);
}

StabilizationMode ActiveBrush::stabilizationMode() const
{
	return m_activeType == CLASSIC ? m_classic.stabilizationMode
								   : m_myPaint.stabilizationMode();
}

void ActiveBrush::setStabilizationMode(StabilizationMode stabilizationMode)
{
	if(m_activeType == CLASSIC) {
		m_classic.stabilizationMode = stabilizationMode;
	} else {
		m_myPaint.setStabilizationMode(stabilizationMode);
	}
}

int ActiveBrush::stabilizerSampleCount() const
{
	return m_activeType == CLASSIC ? m_classic.stabilizerSampleCount
								   : m_myPaint.stabilizerSampleCount();
}

void ActiveBrush::setStabilizerSampleCount(int stabilizerSampleCount)
{
	if(m_activeType == CLASSIC) {
		m_classic.stabilizerSampleCount = stabilizerSampleCount;
	} else {
		m_myPaint.setStabilizerSampleCount(stabilizerSampleCount);
	}
}

int ActiveBrush::smoothing() const
{
	return m_activeType == CLASSIC ? m_classic.smoothing
								   : m_myPaint.smoothing();
}

void ActiveBrush::setSmoothing(int smoothing)
{
	if(m_activeType == CLASSIC) {
		m_classic.smoothing = smoothing;
	} else {
		m_myPaint.setSmoothing(smoothing);
	}
}

QByteArray ActiveBrush::toJson(bool includeSlotProperties) const
{
	QJsonObject json{
		{"type", "dp-active"},
		{"version", 1},
		{"active_type", m_activeType == CLASSIC ? "classic" : "mypaint"},
		{"settings",
		 QJsonObject{
			 {"classic", m_classic.toJson()},
			 {"mypaint", m_myPaint.toJson()},
		 }},
	};
	if(includeSlotProperties) {
		json["_slot"] = QJsonObject{
			{"color", qColor().name()},
		};
	}
	return QJsonDocument{json}.toJson(QJsonDocument::Compact);
}

QByteArray ActiveBrush::toExportJson(const QString &description) const
{
	QJsonObject json{
		{"comment", description},
		{"drawpile_version", cmake_config::version()},
		{"group", ""},
		{"parent_brush_name", ""},
		{"version", 3},
	};
	if(m_activeType == CLASSIC) {
		m_classic.exportToJson(json);
	} else {
		m_myPaint.exportToJson(json);
	}
	return QJsonDocument{json}.toJson(QJsonDocument::Indented);
}

QJsonObject ActiveBrush::toShareJson() const
{
	if(m_activeType == CLASSIC) {
		return m_classic.toJson();
	} else {
		return m_myPaint.toJson();
	}
}

ActiveBrush
ActiveBrush::fromJson(const QJsonObject &json, bool includeSlotProperties)
{
	ActiveBrush brush;
	const QJsonValue type = json["type"];
	if(type == "dp-active") {
		brush.setActiveType(
			json["active_type"] == "mypaint" ? MYPAINT : CLASSIC);
		const QJsonObject o = json["settings"].toObject();
		brush.setClassic(ClassicBrush::fromJson(o["classic"].toObject()));
		brush.setMyPaint(MyPaintBrush::fromJson(o["mypaint"].toObject()));
	} else if(type == "dp-classic") {
		brush.setActiveType(CLASSIC);
		brush.setClassic(ClassicBrush::fromJson(json));
	} else if(type == "dp-mypaint") {
		brush.setActiveType(MYPAINT);
		brush.setMyPaint(MyPaintBrush::fromJson(json));
	} else {
		qWarning("ActiveBrush::fromJson: type is neither dp-active, dp-classic "
				 "nor dp-mypaint!");
	}

	if(includeSlotProperties) {
		QJsonObject slot = json["_slot"].toObject();
		if(!slot.isEmpty()) {
			QColor color = QColor(slot["color"].toString());
			brush.setQColor(color.isValid() ? color : Qt::black);
		}
	}

	return brush;
}

bool ActiveBrush::fromExportJson(const QJsonObject &json)
{
	if(json["drawpile_classic_settings"].isObject()) {
		setActiveType(CLASSIC);
		return m_classic.fromExportJson(json);
	} else {
		setActiveType(MYPAINT);
		return m_myPaint.fromExportJson(json);
	}
}

QString ActiveBrush::presetType() const
{
	return m_activeType == MYPAINT ? QStringLiteral("mypaint")
								   : QStringLiteral("classic");
}

QByteArray ActiveBrush::presetData() const
{
	QJsonObject json =
		m_activeType == MYPAINT ? m_myPaint.toJson() : m_classic.toJson();
	return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QPixmap ActiveBrush::presetThumbnail() const
{
	return m_activeType == MYPAINT ? m_myPaint.presetThumbnail()
								   : m_classic.presetThumbnail();
}

void ActiveBrush::setInBrushEngine(
	drawdance::BrushEngine &be, const DP_StrokeParams &stroke) const
{
	if(m_activeType == CLASSIC) {
		be.setClassicBrush(m_classic, stroke, isEraserOverride());
	} else {
		be.setMyPaintBrush(
			m_myPaint.constBrush(), m_myPaint.constSettings(), stroke,
			isEraserOverride());
	}
}

void ActiveBrush::renderPreview(
	drawdance::BrushPreview &bp, DP_BrushPreviewShape shape) const
{
	if(m_activeType == CLASSIC) {
		bp.renderClassic(m_classic, shape);
	} else {
		bp.renderMyPaint(
			m_myPaint.constBrush(), m_myPaint.constSettings(), shape);
	}
}

}
