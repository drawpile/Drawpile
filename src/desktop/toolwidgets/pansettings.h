// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_PANSETTINGS_H
#define DESKTOP_TOOLWIDGETS_PANSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

namespace tools {

class PanSettings final : public ToolSettings {
	Q_OBJECT
public:
	PanSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("pan"); }

protected:
	QWidget *createUiWidget(QWidget *parent) override;
};

}

#endif
