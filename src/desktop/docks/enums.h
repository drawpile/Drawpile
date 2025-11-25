// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_ENUMS_H
#define DESKTOP_DOCKS_ENUMS_H

#define COLOR_SWATCH_NO_CIRCLE (1 << 0)
#define COLOR_SWATCH_NO_PALETTE (1 << 1)
#define COLOR_SWATCH_NO_SLIDERS (1 << 2)
#define COLOR_SWATCH_NO_SPINNER (1 << 3)
#define COLOR_SWATCH_NONE                                                      \
	(COLOR_SWATCH_NO_CIRCLE | COLOR_SWATCH_NO_PALETTE |                        \
	 COLOR_SWATCH_NO_SLIDERS | COLOR_SWATCH_NO_SPINNER)

#endif
