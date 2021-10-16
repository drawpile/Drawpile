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
#ifndef TOOLSETTINGS_BRUSHES_H
#define TOOLSETTINGS_BRUSHES_H

#include "canvas/pressure.h"
#include "toolsettings.h"

class QAction;

namespace brushes {
	struct ClassicBrush;
}

namespace input {
	struct Preset;
}

namespace tools {

/**
 * @brief Brush settings
 *
 * This is a settings class for the brush tool.
 */
class BrushSettings : public ToolSettings {
	Q_OBJECT
	friend class AdvancedBrushSettings;
public:
	BrushSettings(ToolController *ctrl, QObject *parent=nullptr);
	~BrushSettings();

	QString toolType() const override { return QStringLiteral("brush"); }

	void setActiveTool(tools::Tool::Type tool) override;
	void setForeground(const QColor& color) override;
	void quickAdjust1(qreal adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override;
	bool isSquare() const override;

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setCurrentBrush(brushes::ClassicBrush brush);
	brushes::ClassicBrush currentBrush() const;

	int currentBrushSlot() const;
	bool isCurrentEraserSlot() const;

	void setShareBrushSlotColor(bool sameColor);

	int getSmoothing() const;
	PressureMapping getPressureMapping() const;

	QWidget *getHeaderWidget() override;

	static const int MAX_BRUSH_SIZE;
	static const int MAX_BRUSH_SPACING;
	static const int DEFAULT_BRUSH_SIZE;
	static const int DEFAULT_BRUSH_SPACING;

public slots:
	void selectBrushSlot(int i);
	void selectEraserSlot(bool eraser);
	void toggleEraserMode() override;

signals:
	void colorChanged(const QColor &color);
	void eraseModeChanged(bool erase);
	void subpixelModeChanged(bool subpixel, bool square);
	void smoothingChanged(int smoothing);
	void pressureMappingChanged(const PressureMapping &pressureMapping);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private slots:
	void selectBlendMode(int);
	void setEraserMode(bool erase);
	void updateUi();
	void updateFromUi();
	void chooseInputPreset(int index);
	void updateSettings();

private:
	void emitPresetChanges(const input::Preset *preset);

	struct Private;
	Private *d;
};

}

#endif

