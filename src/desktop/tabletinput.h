// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TABLETINPUT_H
#define TABLETINPUT_H

#include <QMetaType>
#include <QString>

class DrawpileApp;

namespace tabletinput {
Q_NAMESPACE

enum class Mode : int {
	Uninitialized, // Must be the first value, used for a range check.
	KisTabletWinink,
	KisTabletWintab,
	KisTabletWintabRelativePenHack,
	// Only Qt6 allows for toggling between Windows Ink and Wintab (barely,
	// through private headers), Qt5 always uses Windows Ink if it's available.
	Qt6Winink,
	Qt6Wintab,
	Qt5,
	Last = Qt5, // Must be equal to the last value, used for a range check.
};

Q_ENUM_NS(Mode)

void init(DrawpileApp &app);

const char *current();

// When KIS_TABLET isn't enabled Qt will only generate mouse events when a
// tablet input goes unaccepted. We need those mouse events though, since it
// e.g. causes the cursor to update when the cursor enters the canvas or
// unfocuses the chat when you start drawing or panning with a tablet pen. So in
// that case, we don't accept those tablet events and instead add a check in our
// mouse event handlers to disregard synthetically generated mouse events,
// meaning we don't double up on them and the view acts properly.
#ifdef Q_OS_WIN
bool passPenEvents();
#else
constexpr bool passPenEvents()
{
	return true;
}
#endif

}

#endif
