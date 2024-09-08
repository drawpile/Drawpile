// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_BRUSHPALETTEDOCK_H
#define DESKTOP_DOCKS_BRUSHPALETTEDOCK_H
#include "desktop/docks/dockbase.h"

class QTemporaryFile;

namespace brushes {
class ActiveBrush;
}

namespace dialogs {
class BrushExportDialog;
class BrushSettingsDialog;
}

namespace tools {
class BrushSettings;
class ToolProperties;
}

namespace docks {

/**
 * @brief A brush palette dock
 *
 * This dock displays a list of brush presets to choose from.
 */
class BrushPalette final : public DockBase {
	Q_OBJECT
public:
	BrushPalette(QWidget *parent = nullptr);
	~BrushPalette() override;

	void connectBrushSettings(tools::BrushSettings *brushSettings);

	void newPreset();
	void overwriteCurrentPreset(QWidget *parent);

public slots:
	void importBrushes();
	void exportBrushes();

signals:
	void editBrushRequested();

private slots:
	void tagIndexChanged(int proxyRow);
	void setSelectedPresetId(int presetId);
	void prepareTagAssignmentMenu();
	void presetsReset();
	void presetCurrentIndexChanged(
		const QModelIndex &current, const QModelIndex &previous);
	void newTag();
	void editCurrentTag();
	void deleteCurrentTag();
	void resetCurrentPreset();
	void deleteCurrentPreset();
	void exportCurrentTag();
	void exportCurrentPreset();
	void showPresetContextMenu(const QPoint &pos);
	void applyToDetachedBrushSettings(const QModelIndex &proxyIndex);

private:
	struct Private;
	Private *d;

	void applyToBrushSettings(const QModelIndex &proxyIndex);
	void changeTagAssignment(int tagId, bool assigned);

	int tagIdToProxyRow(int tagId);

	int presetRowToSource(int proxyRow);
	int presetRowToProxy(int sourceRow);
	QModelIndex presetIndexToSource(const QModelIndex &proxyIndex);
	QModelIndex presetIndexToProxy(const QModelIndex &sourceIndex);
	int presetProxyIndexToId(const QModelIndex &proxyIndex);
	void updateSelectedPreset();

	void onOpen(const QString &path, QTemporaryFile *tempFile);
	dialogs::BrushExportDialog *showExportDialog();
};

}

#endif
