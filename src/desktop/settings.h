// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SETTINGS_H
#define DESKTOP_SETTINGS_H
#include "desktop/scene/canvasview.h"
#include "desktop/tabletinput.h"
#include "libclient/export/videoexporter.h"
#include "libclient/settings.h"
#include "libclient/tools/tool.h"
#include "libclient/view/enums.h"
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QSize>
#include <QVariant>
#include <QtColorWidgets/ColorWheel>
#include <optional>
#include <utility>

namespace desktop {
Q_NAMESPACE
namespace settings {
Q_NAMESPACE

enum class ThemePalette : int {
	System,
	Light,
	Dark,
	KritaBright,
	KritaDark,
	KritaDarker,
	Fusion,
	HotdogStand,
	Indigo,
	PoolTable,
	Rust,
	BlueApatite,
	OceanDeep,
	RoseQuartz,
	Watermelon,
};
Q_ENUM_NS(ThemePalette)

class Settings final : public libclient::settings::Settings {
	Q_OBJECT

public:
	Settings(QObject *parent = nullptr);

#define DP_SETTINGS_HEADER
#include "desktop/settings_table.h"
#undef DP_SETTINGS_HEADER
};

void initializeTypes();

}
}

#endif
