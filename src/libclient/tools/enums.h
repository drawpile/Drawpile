// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_ENUMS_H
#define LIBCLIENT_TOOLS_ENUMS_H

namespace tools {

enum class ColorPickSource { Canvas, Tool, Touch, Adjust };
enum class DeviceType { Mouse, Tablet, Touch };
enum class QuickAdjustType { Tool, ColorH, ColorS, ColorV };
enum class ToolState { Normal, Busy };

}

#endif
