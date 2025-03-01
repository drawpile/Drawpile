// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_ENUMS_H
#define LIBCLIENT_TOOLS_ENUMS_H
#include <QFlags>

namespace tools {

enum class ColorPickSource { Canvas, Tool, Touch, Adjust };
enum class DeviceType { Mouse, Tablet, Touch };
enum class QuickAdjustType { Tool, ColorH, ColorS, ColorV };
enum class ToolState { Normal, Busy };
enum class Capability : unsigned int {
	AllowColorPick = (1u << 0u),
	AllowToolAdjust = (1u << 1u),
	HandlesRightClick = (1u << 2u),
	Fractional = (1u << 3u),
	SupportsPressure = (1u << 4u),
	IgnoresSelections = (1u << 5u),
	SendsNoMessages = (1u << 6u),
};
Q_DECLARE_FLAGS(Capabilities, Capability)

}

Q_DECLARE_OPERATORS_FOR_FLAGS(tools::Capabilities)

#endif
