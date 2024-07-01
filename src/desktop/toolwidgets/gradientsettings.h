// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_GRADIENTSETTINGS_H
#define DESKTOP_TOOLWIDGETS_GRADIENTSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

namespace tools {

class GradientSettings final : public ToolSettings {
	Q_OBJECT
public:
	GradientSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("gradient"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }

protected:
	QWidget *createUiWidget(QWidget *parent) override;
};

}

#endif
