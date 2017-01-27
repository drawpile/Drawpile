/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2016 Calle Laakkonen

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

#include "toolsettings.h"

class Ui_PenSettings;
class Ui_BrushSettings;
class Ui_SmudgeSettings;
class Ui_EraserSettings;

namespace tools {

/**
 * @brief Pen settings
 *
 * This is much like BrushSettings, except the pen always has 100% hardness
 * and no antialiasing.
 */
class PenSettings : public ToolSettings {
public:
	PenSettings(QString name, QString title, ToolController *ctrl);
	~PenSettings();

	tools::Tool::Type toolType() const override { return tools::Tool::PEN; }

	void setForeground(const QColor& color) override;
	void quickAdjust1(float adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override { return false; }

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_PenSettings *_ui;
};

/**
 * @brief Eraser settings
 *
 * This is a settings class for brushes that erase.
 * Erasers don't actually use the colors assigned to them, but will
 * always simple erase the alpha channel.
 */
class EraserSettings : public ToolSettings {
public:
	EraserSettings(QString name, QString title, ToolController *ctrl);
	~EraserSettings();

	tools::Tool::Type toolType() const override { return tools::Tool::ERASER; }

	void setForeground(const QColor& color) override;
	void quickAdjust1(float adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override;

	void pushSettings() override;
	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_EraserSettings *_ui;
};

/**
 * @brief Brush settings
 *
 * This is a settings class for the brush tool.
 */
class BrushSettings : public ToolSettings {
public:
	BrushSettings(QString name, QString title, ToolController *ctrl);
	~BrushSettings();

	tools::Tool::Type toolType() const override { return tools::Tool::BRUSH; }

	void setForeground(const QColor& color) override;
	void quickAdjust1(float adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override { return true; }

	void pushSettings() override;
	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_BrushSettings *_ui;
};

/**
 * @brief Smudge brush settings
 *
 * This is a settings class for the color smudging brush tool.
 */
class SmudgeSettings : public ToolSettings {
public:
	SmudgeSettings(QString name, QString title, ToolController *ctrl);
	~SmudgeSettings();

	tools::Tool::Type toolType() const override { return tools::Tool::SMUDGE; }

	void setForeground(const QColor& color) override;
	void quickAdjust1(float adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override { return true; }

	void pushSettings() override;
	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_SmudgeSettings *_ui;
};

}

#endif

