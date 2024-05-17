// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_ZOOM_H
#define TOOLSETTINGS_ZOOM_H

#include "desktop/toolwidgets/toolsettings.h"

namespace tools {

/**
 * @brief Zoom tool options
 */
class ZoomSettings final : public ToolSettings {
	Q_OBJECT
public:
	ZoomSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("zoom"); }

signals:
	void resetZoom();
	void fitToWindow();

protected:
	QWidget *createUiWidget(QWidget *parent) override;
};

}

#endif
