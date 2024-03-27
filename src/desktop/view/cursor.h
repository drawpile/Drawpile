// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CURSOR_H
#define DESKTOP_VIEW_CURSOR_H

namespace view {

enum class Cursor : int {
	Dot,
	Cross,
	Arrow,
	TriangleRight,
	TriangleLeft,
	Eraser,
	SameAsBrush = -1,
};

}

#endif
