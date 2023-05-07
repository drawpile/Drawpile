// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLORSPINNERDOCK_H
#define COLORSPINNERDOCK_H

#include <QDockWidget>

namespace color_widgets {
	class ColorPalette;
}

namespace docks {

class ColorSpinnerDock final : public QDockWidget {
	Q_OBJECT
public:
	ColorSpinnerDock(const QString& title, QWidget *parent);
	~ColorSpinnerDock() override;

public slots:
	void setColor(const QColor& color);
	void setLastUsedColors(const color_widgets::ColorPalette &pal);

signals:
	void colorSelected(const QColor& color);

private:
	struct Private;
	Private *d;
};

}

#endif

