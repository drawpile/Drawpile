/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "libclient/brushes/brush.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/drawdance/brushengine.h"
#include "libclient/utils/icon.h"
#include "libshared/util/qtcompat.h"

#include <cmath>
#include <dpengine/libmypaint/mypaint-brush.h>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

void setDrawdanceColorToQColor(DP_UPixelFloat &r, const QColor &q)
{
	r = {compat::cast<float>(q.blueF()), compat::cast<float>(q.greenF()), compat::cast<float>(q.redF()), compat::cast<float>(q.alphaF())};
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
		false,
		false,
		false,
		false,
	}
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

void ClassicBrush::setQColor(const QColor& c)
{
	setDrawdanceColorToQColor(color, c);
}

QColor ClassicBrush::qColor() const
{
	return drawdanceColorToQColor(color);
}

QJsonObject ClassicBrush::toJson() const
{
	QJsonObject o;
	switch(shape) {
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND: o["shape"] = "round-pixel"; break;
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE: o["shape"] = "square-pixel"; break;
	default: o["shape"] = "round-soft"; break;
	}

	o["size"] = size.max;
	if(size.min > 0) o["size2"] = size.min;
	o["sizecurve"] = m_sizeCurve.toString();

	o["opacity"] = opacity.max;
	if(opacity.min > 0) o["opacity2"] = opacity.min;
	o["opacitycurve"] = m_opacityCurve.toString();

	o["hard"] = hardness.max;
	if(hardness.min > 0) o["hard2"] = hardness.min;
	o["hardcurve"] = m_hardnessCurve.toString();

	if(smudge.max > 0) o["smudge"] = smudge.max;
	if(smudge.min > 0) o["smudgeratio"] = smudge.min;
	o["smudgecurve"] = m_smudgeCurve.toString();

	o["spacing"] = spacing;
	if(resmudge>0) o["resmudge"] = resmudge;

	if(!incremental) o["indirect"] = true;
	if(colorpick) o["colorpick"] = true;
	if(size_pressure) o["sizep"] = true;
	if(hardness_pressure) o["hardp"] = true;
	if(opacity_pressure) o["opacityp"] = true;
	if(smudge_pressure) o["smudgep"] = true;

	o["blend"] = canvas::blendmode::svgName(brush_mode);
	o["blenderase"] = canvas::blendmode::svgName(erase_mode);

	// Note: color is intentionally omitted

	return QJsonObject {
		{"type", "dp-classic"},
		{"version", 1},
		{"settings", o}
	};
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
	b.size_pressure = o["sizep"].toBool();
	b.hardness_pressure = o["hardp"].toBool();
	b.opacity_pressure = o["opacityp"].toBool();
	b.smudge_pressure = o["smudgep"].toBool();

	b.brush_mode = canvas::blendmode::fromSvgName(o["blend"].toString());
	b.erase_mode = canvas::blendmode::fromSvgName(
		o["blenderase"].toString(), DP_BLEND_MODE_ERASE);

	return b;
}

QPixmap ClassicBrush::presetThumbnail() const
{
	QColor c;
	if(smudge.max > 0.0f) {
		c = QColor::fromRgbF(0.9f, 0.6f, 0.1f);
	} else if(icon::isDarkThemeSelected()) {
		c = Qt::white;
	} else {
		c = Qt::black;
	}
	return drawdance::BrushPreview::classicBrushPreviewDab(*this, 64, 64, c);
}

void ClassicBrush::updateCurve(const KisCubicCurve &src, DP_ClassicBrushCurve &dst)
{
	const QVector<qreal> &values = src.floatTransfer(DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT);
	for(int i = 0; i < DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT; ++i) {
		dst.values[i] = values[i];
	}
}


MyPaintBrush::MyPaintBrush()
	: m_brush{{0.0f, 0.0f, 0.0f, 1.0f}, false, false}
	, m_settings{nullptr}
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
	, m_curves{other.m_curves}
{
	other.m_settings = nullptr;
}

MyPaintBrush &MyPaintBrush::operator=(MyPaintBrush &&other)
{
	std::swap(m_brush, other.m_brush);
	std::swap(m_settings, other.m_settings);
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

void MyPaintBrush::setQColor(const QColor& c)
{
	setDrawdanceColorToQColor(m_brush.color, c);
}

QColor MyPaintBrush::qColor() const
{
	return drawdanceColorToQColor(m_brush.color);
}

QJsonObject MyPaintBrush::toJson() const
{
	QJsonValue jsonMapping;
	if(m_settings) {
		QJsonObject o;
		for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
			const DP_MyPaintMapping &mapping = m_settings->mappings[i];

			QJsonObject inputs;
			for (int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
				const DP_MyPaintControlPoints &cps = mapping.inputs[j];
				if (cps.n) {
					QJsonArray points;
					for (int k = 0; k < cps.n; ++k) {
						points.append(QJsonArray {
							cps.xvalues[k],
							cps.yvalues[k],
						});
					}
					MyPaintBrushInput inputId = static_cast<MyPaintBrushInput>(j);
					inputs[mypaint_brush_input_info(inputId)->cname] = points;
				}
			}

			MyPaintBrushSetting settingId = static_cast<MyPaintBrushSetting>(i);
			o[mypaint_brush_setting_info(settingId)->cname] = QJsonObject {
				{"base_value", mapping.base_value},
				{"inputs", inputs},
			};
		}
		jsonMapping = o;
	}

	return QJsonObject {
		{"type", "dp-mypaint"},
		{"version", 1},
		{"settings", QJsonObject {
			{"lock_alpha", m_brush.lock_alpha},
			{"erase", m_brush.erase},
			{"mapping", jsonMapping},
		}},
	};
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
	b.loadJsonSettings(o["mapping"].toObject());

	return b;
}

bool MyPaintBrush::loadMyPaintJson(const QJsonObject &json)
{
	if(json["version"].toInt() != 3) {
		qWarning("MyPaintBrush::fromMyPaintJson: version not 3");
		return false;
	}

	if(loadJsonSettings(json["settings"].toObject())) {
		m_brush.lock_alpha = false;
		m_brush.erase = false;
		return true;
	} else {
		return false;
	}
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
        for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
            DP_MyPaintMapping &mapping = settings.mappings[i];
            mapping.base_value = mypaint_brush_setting_info(
				static_cast<MyPaintBrushSetting>(i))->def;
            for (int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
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

bool MyPaintBrush::loadJsonSettings(const QJsonObject &o)
{
	bool foundSetting = false;
	for (const QString &mappingKey : o.keys()) {
		int settingId = mypaint_brush_setting_from_cname(mappingKey.toUtf8().constData());
		if(settingId < 0 || settingId >= MYPAINT_BRUSH_SETTINGS_COUNT) {
			qWarning("Unknown MyPaint brush setting '%s'", qPrintable(mappingKey));
		} else if (loadJsonMapping(mappingKey, settingId, o[mappingKey].toObject())) {
			foundSetting = true;
		}
	}
	return foundSetting;
}

bool MyPaintBrush::loadJsonMapping(const QString &mappingKey,
	int settingId, const QJsonObject &o)
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
	} else if (loadJsonInputs(mappingKey, settingId, o["inputs"].toObject())) {
		foundSetting = true;
	}

	return foundSetting;
}

bool MyPaintBrush::loadJsonInputs(const QString &mappingKey,
	int settingId, const QJsonObject &o)
{
	bool foundSetting = false;

	for (const QString &inputKey : o.keys()) {
		int inputId = mypaint_brush_input_from_cname(inputKey.toUtf8().constData());
		if(inputId < 0 || inputId >= MYPAINT_BRUSH_INPUTS_COUNT) {
			qWarning("Unknown MyPaint brush input '%s' to %s",
				qPrintable(inputKey), qPrintable(mappingKey));
		} else if (!o[inputKey].isArray()) {
			qWarning("MyPaint input '%s' in %s is not an array",
				qPrintable(inputKey), qPrintable(mappingKey));
		} else {
			foundSetting = true;
			DP_MyPaintControlPoints &cps = settings().mappings[settingId].inputs[inputId];
			const QJsonArray points = o[inputKey].toArray();

			static constexpr int MAX_POINTS = sizeof(cps.xvalues) / sizeof(cps.xvalues[0]);
			cps.n = qMin(MAX_POINTS, points.count());

			for (int i = 0; i < cps.n; ++i) {
				const QJsonArray point = points[i].toArray();
				cps.xvalues[i] = point.at(0).toDouble();
				cps.yvalues[i] = point.at(1).toDouble();
			}
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
	return m_activeType == CLASSIC ? m_classic.erase : m_myPaint.constBrush().erase;
}

DP_UPixelFloat ActiveBrush::color() const
{
	return m_activeType == CLASSIC ? m_classic.color : m_myPaint.constBrush().color;
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


QJsonObject ActiveBrush::toJson() const
{
	return QJsonObject {
		{"type", "dp-active"},
		{"version", 1},
		{"active_type", m_activeType == CLASSIC ? "classic" : "mypaint"},
		{"settings", QJsonObject {
			{"classic", m_classic.toJson()},
			{"mypaint", m_myPaint.toJson()},
		}},
	};
}

ActiveBrush ActiveBrush::fromJson(const QJsonObject &json)
{
	ActiveBrush brush;
	const QJsonValue type = json["type"];
	if(type == "dp-active") {
		brush.setActiveType(json["active_type"] == "mypaint" ? MYPAINT : CLASSIC);
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
		qWarning("ActiveBrush::fromJson: type is neither dp-active, dp-classic nor dp-mypaint!");
	}
	return brush;
}

QString ActiveBrush::presetType() const
{
	return m_activeType == MYPAINT ? QStringLiteral("mypaint") : QStringLiteral("classic");
}

QByteArray ActiveBrush::presetData() const
{
	QJsonObject json = m_activeType == MYPAINT ? m_myPaint.toJson() : m_classic.toJson();
	return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QPixmap ActiveBrush::presetThumbnail() const
{
	return m_activeType == MYPAINT ? m_myPaint.presetThumbnail() : m_classic.presetThumbnail();
}

void ActiveBrush::setInBrushEngine(drawdance::BrushEngine &be, uint16_t layer, bool freehand) const
{
	if(m_activeType == CLASSIC) {
		be.setClassicBrush(m_classic, layer);
	} else {
		be.setMyPaintBrush(
			m_myPaint.constBrush(), m_myPaint.constSettings(), layer, freehand);
	}
}

void ActiveBrush::renderPreview(drawdance::BrushPreview &bp, DP_BrushPreviewShape shape) const
{
	if(m_activeType == CLASSIC) {
		bp.renderClassic(m_classic, shape);
	} else {
		bp.renderMyPaint(m_myPaint.constBrush(), m_myPaint.constSettings(), shape);
	}
}

}

