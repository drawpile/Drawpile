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

#include "../../rustpile/rustpile.h"

#include <QColor>
#include <QJsonObject>
#include <QMetaType>
#include <QPixmap>

struct MyPaintBrush;

namespace brushes {

//! A convenience wrapper for classic brush settings
struct ClassicBrush : public rustpile::ClassicBrush
{
	ClassicBrush();

	bool isEraser() const;
	void setQColor(const QColor& c);
	QColor qColor() const;

	QJsonObject toJson() const;
	static ClassicBrush fromJson(const QJsonObject &json);

	QPixmap presetThumbnail() const;
};

struct MyPaintBrush final
{
public:
	MyPaintBrush();
	~MyPaintBrush();

    MyPaintBrush(const MyPaintBrush &other);
	MyPaintBrush(MyPaintBrush &&other);

    MyPaintBrush &operator=(MyPaintBrush &&other);
    MyPaintBrush &operator=(const MyPaintBrush &other);

	rustpile::MyPaintBrush &brush() { return m_brush; }
	const rustpile::MyPaintBrush &constBrush() const { return m_brush; }

	rustpile::MyPaintSettings &settings();
	const rustpile::MyPaintSettings &constSettings() const;

	void setQColor(const QColor& c);
	QColor qColor() const;

	QJsonObject toJson() const;
	static MyPaintBrush fromJson(const QJsonObject &json);

	bool loadMyPaintJson(const QJsonObject &json);

	QPixmap presetThumbnail() const;

private:
	rustpile::MyPaintBrush m_brush;
	rustpile::MyPaintSettings *m_settings;

	static const rustpile::MyPaintSettings &getDefaultSettings();

	bool loadJsonSettings(const QJsonObject &o);
	bool loadJsonMapping(const QString &mappingKey, int settingId, const QJsonObject &o);
	bool loadJsonInputs(const QString &mappingKey, int settingId, const QJsonObject &o);
};

struct ActiveBrush final
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

	ClassicBrush &classic() { return m_classic; }
	const ClassicBrush &classic() const { return m_classic; }
	void setClassic(const ClassicBrush &classic) { m_classic = classic; }

	MyPaintBrush &myPaint() { return m_myPaint; }
	const MyPaintBrush &myPaint() const { return m_myPaint; }
	void setMyPaint(const MyPaintBrush &myPaint) { m_myPaint = myPaint; }

	bool isEraser() const;

	rustpile::Color color() const;
	QColor qColor() const;
	void setQColor(const QColor &c);

	QJsonObject toJson() const;
	static ActiveBrush fromJson(const QJsonObject &json);

	QString presetType() const;
	QByteArray presetData() const;
	QPixmap presetThumbnail() const;

	void setInBrushEngine(rustpile::BrushEngine *be, uint16_t layer, bool freehand = true) const;
	void renderPreview(rustpile::BrushPreview *bp, rustpile::BrushPreviewShape shape) const;

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


