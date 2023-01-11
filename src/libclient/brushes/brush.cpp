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

#include "brush.h"
#include "canvas/blendmodes.h"
#include "../utils/icon.h"

#include <cmath>
#include <mypaint-brush.h>
#include <mypaint-brush-settings.h>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

void setRustpileColorToQColor(rustpile::Color &r, const QColor &q)
{
	r = {float(q.redF()), float(q.greenF()), float(q.blueF()), float(q.alphaF())};
}

QColor rustpileColorToQColor(const rustpile::Color &color)
{
	return QColor::fromRgbF(color.r, color.g, color.b, color.a);
}

}

namespace brushes {

ClassicBrush::ClassicBrush()
	: rustpile::ClassicBrush{
		{1.0, 10.0},
		{0, 1},
		{0, 1},
		{0, 0},
		0.1f,
		0,
		rustpile::Color_BLACK,
		rustpile::ClassicBrushShape::RoundPixel,
		rustpile::Blendmode::Normal,
		true,
		false,
		false,
		false,
		false,
		false
	}
{
}

bool ClassicBrush::isEraser() const
{
	return mode == rustpile::Blendmode::Erase || mode == rustpile::Blendmode::ColorErase;
}

void ClassicBrush::setQColor(const QColor& c)
{
	setRustpileColorToQColor(color, c);
}

QColor ClassicBrush::qColor() const
{
	return rustpileColorToQColor(color);
}

QJsonObject ClassicBrush::toJson() const
{
	QJsonObject o;
	switch(shape) {
	case rustpile::ClassicBrushShape::RoundPixel: o["shape"] = "round-pixel"; break;
	case rustpile::ClassicBrushShape::SquarePixel: o["shape"] = "square-pixel"; break;
	case rustpile::ClassicBrushShape::RoundSoft: o["shape"] = "round-soft"; break;
	}

	o["size"] = size.max;
	if(size.min>1) o["size2"] = size.min;

	o["opacity"] = opacity.max;
	if(opacity.min>0) o["opacity2"] = opacity.min;

	o["hard"] = hardness.max;
	if(hardness.min>0) o["hard2"] = hardness.min;

	if(smudge.max > 0) o["smudge"] = smudge.max;
	if(smudge.min > 0) o["smudge2"] = smudge.min;

	o["spacing"] = spacing;
	if(resmudge>0) o["resmudge"] = resmudge;

	if(incremental) o["inc"] = true;
	if(colorpick) o["colorpick"] = true;
	if(size_pressure) o["sizep"] = true;
	if(hardness_pressure) o["hardp"] = true;
	if(opacity_pressure) o["opacityp"] = true;
	if(smudge_pressure) o["smudgep"] = true;

	o["blend"] = canvas::blendmode::svgName(mode);

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
		b.shape = rustpile::ClassicBrushShape::RoundPixel;
	else if(o["shape"] == "square-pixel")
		b.shape = rustpile::ClassicBrushShape::SquarePixel;
	else
		b.shape = rustpile::ClassicBrushShape::RoundSoft;

	b.size.max = o["size"].toDouble();
	b.size.min = o["size2"].toDouble();

	b.opacity.max = o["opacity"].toDouble();
	b.opacity.min= o["opacity2"].toDouble();

	b.hardness.max = o["hard"].toDouble();
	b.hardness.min = o["hard2"].toDouble();

	b.smudge.max = o["smudge"].toDouble();
	b.smudge.min = o["smudge2"].toDouble();
	b.resmudge = o["resmudge"].toInt();

	b.spacing = o["spacing"].toDouble();

	b.incremental = o["inc"].toBool();
	b.colorpick = o["colorpick"].toBool();
	b.size_pressure = o["sizep"].toBool();
	b.hardness_pressure = o["hardp"].toBool();
	b.opacity_pressure = o["opacityp"].toBool();
	b.smudge_pressure = o["smudgep"].toBool();

	b.mode = canvas::blendmode::fromSvgName(o["blend"].toString());

	return b;
}

QPixmap ClassicBrush::presetThumbnail() const
{
	QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
	img.fill(0);

	rustpile::Color c;
	if(smudge.max > 0.0f) {
		c = rustpile::Color{0.1f, 0.6f, 0.9f, 1.0};
	} else if(icon::isDarkThemeSelected()) {
		c = rustpile::Color{1.0, 1.0, 1.0, 1.0};
	} else {
		c = rustpile::Color{0.0, 0.0, 0.0, 1.0};
	}

	rustpile::brush_preview_dab(this, img.bits(), img.width(), img.height(), &c);

	QPixmap pixmap;
	pixmap.convertFromImage(img);
	return pixmap;
}


MyPaintBrush::MyPaintBrush()
	: m_brush{rustpile::Color_BLACK, false, false}
	, m_settings(nullptr)
{
	static_assert(
		sizeof(m_settings->mappings) / sizeof(m_settings->mappings[0]) == MYPAINT_BRUSH_SETTINGS_COUNT,
		"Mapping count must match MYPAINT_BRUSH_SETTINGS_COUNT");
	static_assert(
		sizeof(m_settings->mappings->inputs) / sizeof(m_settings->mappings->inputs[0]) == MYPAINT_BRUSH_INPUTS_COUNT,
		"Mapping input count must match MYPAINT_BRUSH_INPUTS_COUNT");
}

MyPaintBrush::~MyPaintBrush()
{
	delete m_settings;
}

MyPaintBrush::MyPaintBrush(const MyPaintBrush &other)
	: m_brush{other.m_brush}
	, m_settings(nullptr)
{
	if(other.m_settings) {
		m_settings = new rustpile::MyPaintSettings;
		*m_settings = *other.m_settings;
	}
}

MyPaintBrush::MyPaintBrush(MyPaintBrush &&other)
	: m_brush{other.m_brush}
	, m_settings(other.m_settings)
{
	other.m_settings = nullptr;
}

MyPaintBrush &MyPaintBrush::operator=(MyPaintBrush &&other)
{
	std::swap(m_brush, other.m_brush);
	std::swap(m_settings, other.m_settings);
	return *this;
}

MyPaintBrush &MyPaintBrush::operator=(const MyPaintBrush &other)
{
	m_brush = other.m_brush;
	if(other.m_settings) {
		if(!m_settings) {
			m_settings = new rustpile::MyPaintSettings;
		}
		*m_settings = *other.m_settings;
	} else {
		delete m_settings;
		m_settings = nullptr;
	}
	return *this;
}

rustpile::MyPaintSettings &MyPaintBrush::settings()
{
	if(!m_settings) {
		m_settings = new rustpile::MyPaintSettings(getDefaultSettings());
	}
	return *m_settings;
}

const rustpile::MyPaintSettings &MyPaintBrush::constSettings() const
{
	return m_settings ? *m_settings : getDefaultSettings();
}

void MyPaintBrush::setQColor(const QColor& c)
{
	setRustpileColorToQColor(m_brush.color, c);
}

QColor MyPaintBrush::qColor() const
{
	return rustpileColorToQColor(m_brush.color);
}

QJsonObject MyPaintBrush::toJson() const
{
	QJsonValue mapping;
	if(m_settings) {
		QJsonObject o;
		for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
			const rustpile::MyPaintMapping &mapping = m_settings->mappings[i];

			QJsonObject inputs;
			for (int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
				const rustpile::ControlPoints &cps = mapping.inputs[j];
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
		mapping = o;
	}

	return QJsonObject {
		{"type", "dp-mypaint"},
		{"version", 1},
		{"settings", QJsonObject {
			{"lock_alpha", m_brush.lock_alpha},
			{"erase", m_brush.erase},
			{"mapping", mapping},
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

const rustpile::MyPaintSettings &MyPaintBrush::getDefaultSettings()
{
	static rustpile::MyPaintSettings settings;
	static bool settingsInitialized;
	if(!settingsInitialized) {
		settingsInitialized = true;
		// Same procedure as mypaint_brush_from_defaults.
        for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
            rustpile::MyPaintMapping &mapping = settings.mappings[i];
            mapping.base_value = mypaint_brush_setting_info(
				static_cast<MyPaintBrushSetting>(i))->def;
            for (int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
                mapping.inputs[j].n = 0;
            }
        }
        rustpile::MyPaintMapping &opaqueMultiplyMapping =
			settings.mappings[MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY];
        rustpile::ControlPoints &brushInputPressure =
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
			rustpile::ControlPoints &cps = settings().mappings[settingId].inputs[inputId];
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

bool ActiveBrush::isEraser() const
{
	return m_activeType == CLASSIC ? m_classic.isEraser() : m_myPaint.constBrush().erase;
}

rustpile::Color ActiveBrush::color() const
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

void ActiveBrush::setInBrushEngine(rustpile::BrushEngine *be, uint16_t layer, bool freehand) const
{
	if(m_activeType == CLASSIC) {
		rustpile::brushengine_set_classicbrush(be, &m_classic, layer);
	} else {
		rustpile::brushengine_set_mypaintbrush(
			be, &m_myPaint.constBrush(), &m_myPaint.constSettings(), layer, freehand);
	}
}

void ActiveBrush::renderPreview(rustpile::BrushPreview *bp, rustpile::BrushPreviewShape shape) const
{
	if(m_activeType == CLASSIC) {
		rustpile::brushpreview_render_classic(bp, &m_classic, shape);
	} else {
		rustpile::brushpreview_render_mypaint(
			bp, &m_myPaint.constBrush(), &m_myPaint.constSettings(), shape);
	}
}

}

