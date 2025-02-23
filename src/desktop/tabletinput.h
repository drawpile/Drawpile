// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESTKOP_TABLETINPUT_H
#define DESTKOP_TABLETINPUT_H
#include <QMetaType>
#include <QString>

class DrawpileApp;

#ifdef Q_OS_WIN
#	define TABLETINPUT_CONSTEXPR_OR_INLINE inline
#else
#	define TABLETINPUT_CONSTEXPR_OR_INLINE constexpr
#endif

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
	KisTabletWininkNonNative,
	Last = KisTabletWininkNonNative, // Must be equal to the last value, used
									 // for a range check.
};

Q_ENUM_NS(Mode)

enum class EraserAction {
	Ignore,
#ifndef __EMSCRIPTEN__
	Switch,
#endif
	Override,
#ifdef __EMSCRIPTEN__
	Default = Override,
#else
	Default = Switch,
#endif
};

Q_ENUM_NS(EraserAction)

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

TABLETINPUT_CONSTEXPR_OR_INLINE bool ignoreSpontaneous()
{
	return !passPenEvents();
}

}

#endif
