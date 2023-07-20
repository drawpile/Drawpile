// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLORPALETTEDOCK_H
#define COLORPALETTEDOCK_H

#include "desktop/docks/dockbase.h"

namespace color_widgets {
class ColorPalette;
}

namespace docks {

class ColorPaletteDock final : public DockBase {
	Q_OBJECT
public:
	ColorPaletteDock(const QString &title, QWidget *parent);
	~ColorPaletteDock() override;

public slots:
	void setColor(const QColor &color);
	void setLastUsedColors(const color_widgets::ColorPalette &pal);

signals:
	void colorSelected(const QColor &color);

private slots:
	void paletteChanged(int index);
	void addColumn();
	void removeColumn();
	void updateColumnButtons(int columns);
	void addPalette();
	void copyPalette();
	void deletePalette();
	void renamePalette();
	void exportPalette();
	void importPalette();
	void selectColor(const QColor &color);

private:
	void showColorDialog(int index, bool replace);

	struct Private;
	Private *d;
};

int findPaletteColor(
	const color_widgets::ColorPalette &pal, const QColor &color);

}

#endif
