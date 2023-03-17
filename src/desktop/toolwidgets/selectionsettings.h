/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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
#ifndef TOOLSETTINGS_SELECTION_H
#define TOOLSETTINGS_SELECTION_H

#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/tools/selection.h"

class Ui_SelectionSettings;

namespace canvas { class CanvasModel; }

namespace widgets { class CanvasView; }

namespace tools {

/**
 * @brief Settings and actions for canvas selections (rectangular and freeform)
 *
 * Unlike most tool settings widgets, this one also includes buttons to trigger
 * various actions on the active selection (e.g. flip/mirror.)
 */
class SelectionSettings final : public ToolSettings {
	Q_OBJECT
public:
	SelectionSettings(ToolController *ctrl, QObject *parent=nullptr);
	~SelectionSettings() override;

	QString toolType() const override { return QStringLiteral("selection"); }

	/**
	 * @brief Set the view widget
	 *
	 * This is used to get the current view rectangle needed by fitToScreen()
	 */
	void setView(widgets::CanvasView *view) { m_view = view; }

	void setForeground(const QColor&) override {}

	int getSize() const override { return 0; }
	bool getSubpixelMode() const override { return false; }

	void setControlsEnabled(bool enabled);

private slots:
	void flipSelection();
	void mirrorSelection();
	void fitToScreen();
	void resetSize();
	void scale();
	void rotateShear();
	void distort();
	void modelChanged(canvas::CanvasModel *model);
	void selectionChanged(canvas::Selection *selection);
	void selectionClosed();
	void selectionAdjustmentModeChanged(canvas::Selection::AdjustmentMode mode);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void updateSelectionMode(canvas::Selection::AdjustmentMode mode);
	void cutSelection();

	Ui_SelectionSettings *m_ui;
	widgets::CanvasView *m_view;
};

}

#endif



