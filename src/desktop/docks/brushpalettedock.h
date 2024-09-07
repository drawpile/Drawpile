// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_BRUSHPALETTEDOCK_H
#define DESKTOP_DOCKS_BRUSHPALETTEDOCK_H

#include "libclient/tools/tool.h"

#include "desktop/docks/dockbase.h"

class QTemporaryFile;

namespace dialogs {
class BrushExportDialog;
}

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
class BrushPalette final : public DockBase {
	Q_OBJECT
public:
	BrushPalette(QWidget *parent = nullptr);
	~BrushPalette() override;

	void connectBrushSettings(tools::ToolSettings *toolSettings);

public slots:
	void importBrushes();
	void exportBrushes();

private slots:
	void tagIndexChanged(int proxyRow);
	void presetsReset();
	void presetSelectionChanged(
		const QModelIndex &current, const QModelIndex &previous);
	void newTag();
	void editCurrentTag();
	void deleteCurrentTag();
	void newPreset();
	void duplicateCurrentPreset();
	void overwriteCurrentPreset();
	void editCurrentPreset();
	void deleteCurrentPreset();
	void exportCurrentTag();
	void exportCurrentPreset();
	void applyPresetProperties(
		int id, const QString &name, const QString &description,
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

	void onOpen(const QString &path, QTemporaryFile *tempFile);
	dialogs::BrushExportDialog *showExportDialog();

	bool question(const QString &title, const QString &text) const;
};

}

#endif
