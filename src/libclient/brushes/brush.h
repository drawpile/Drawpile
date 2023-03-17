/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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
#ifndef DP_BRUSHES_BRUSH_H
#define DP_BRUSHES_BRUSH_H

extern "C" {
#include <dpengine/brush.h>
#include <dpengine/pixels.h>
}

#include "drawdance/brushpreview.h"

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QMetaType>
#include <QPair>
#include <QPixmap>
#include <limits>
#include <utils/kis_cubic_curve.h>

struct MyPaintBrush;

namespace drawdance {
	class BrushEngine;
}

namespace brushes {

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

	QJsonObject toJson() const;
	static ActiveBrush fromJson(const QJsonObject &json);

	QString presetType() const;
	QByteArray presetData() const;
	QPixmap presetThumbnail() const;

	void setInBrushEngine(drawdance::BrushEngine &be, uint16_t layer, bool freehand = true) const;
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


