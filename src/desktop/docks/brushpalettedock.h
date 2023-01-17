/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2022 Calle Laakkonen, askmeaboutloom

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
#ifndef BRUSHPALETTEDOCK_H
#define BRUSHPALETTEDOCK_H

#include "libclient/tools/tool.h"

#include <QDockWidget>

namespace tools {
	class ToolProperties;
	class ToolSettings;
}

namespace docks {

/**
 * @brief A brush palette dock
 *
 * This dock displays a list of brush presets to choose from.
 */
class BrushPalette final : public QDockWidget
{
Q_OBJECT
public:
	BrushPalette(QWidget *parent=nullptr);
	~BrushPalette() override;

	void connectBrushSettings(tools::ToolSettings *toolSettings);

private slots:
   void tagIndexChanged(int proxyRow);
   void presetsReset();
   void presetSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
   void newTag();
   void editCurrentTag();
   void deleteCurrentTag();
   void newPreset();
   void duplicateCurrentPreset();
   void overwriteCurrentPreset();
   void editCurrentPreset();
   void deleteCurrentPreset();
   void importMyPaintBrushes();
   void applyPresetProperties(int id, const QString &name, const QString &description,
        const QPixmap &thumbnail);
   void applyToBrushSettings(const QModelIndex &index);
   void showPresetContextMenu(const QPoint &pos);

private:
	struct Private;
	Private *d;

   void changeTagAssignment(int tagId, bool assigned);

   int tagIdToProxyRow(int tagId);

   int presetRowToSource(int proxyRow);
   int presetRowToProxy(int sourceRow);
   QModelIndex presetIndexToSource(const QModelIndex &proxyIndex);
   QModelIndex presetIndexToProxy(const QModelIndex &sourceIndex);
   int presetProxyIndexToId(const QModelIndex &proxyIndex);
   int currentPresetId();
};

}

#endif

