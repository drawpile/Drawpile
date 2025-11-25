// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_VIEW_ENUMS_H
#define LIBCLIENT_VIEW_ENUMS_H
#include <QObject>

#ifndef ONE_FINGER_TOUCH_DEFAULT
#	if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#		define ONE_FINGER_TOUCH_DEFAULT view::OneFingerTouchAction::Guess
#	else
#		define ONE_FINGER_TOUCH_DEFAULT view::OneFingerTouchAction::Pan
#	endif
#endif

namespace view {
Q_NAMESPACE

enum class CanvasImplementation : int {
	Default = 0,
	GraphicsView = 1,
	OpenGl = 2,
	Software = 3,
};
Q_ENUM_NS(CanvasImplementation)

#if defined(__EMSCRIPTEN__)
#	define CANVAS_IMPLEMENTATION_DEFAULT ::view::CanvasImplementation::OpenGl
#	undef CANVAS_IMPLEMENTATION_FALLBACK
#elif defined(Q_OS_ANDROID)
#	define CANVAS_IMPLEMENTATION_DEFAULT ::view::CanvasImplementation::OpenGl
#	define CANVAS_IMPLEMENTATION_FALLBACK                                     \
		::view::CanvasImplementation::GraphicsView
#else
#	define CANVAS_IMPLEMENTATION_DEFAULT                                      \
		::view::CanvasImplementation::GraphicsView
#	define CANVAS_IMPLEMENTATION_FALLBACK ::view::CanvasImplementation::Default
#endif

enum class Cursor : int {
	Dot,
	Cross,
	Arrow,
	TriangleRight,
	TriangleLeft,
	Eraser,
	Blank,
	SameAsBrush = -1,
};
Q_ENUM_NS(Cursor)

enum class InterfaceMode : int {
	Dynamic,
	Desktop,
	SmallScreen,
};
Q_ENUM_NS(InterfaceMode)

enum class KineticScrollGesture : int {
	None,
	LeftClick,
	MiddleClick,
	RightClick,
	Touch,
};
Q_ENUM_NS(KineticScrollGesture)

enum class OneFingerTouchAction : int {
	Nothing,
	Pan,
	Draw,
	Guess,
};
Q_ENUM_NS(OneFingerTouchAction)

enum class TwoFingerPinchAction : int {
	Nothing,
	Zoom,
};
Q_ENUM_NS(TwoFingerPinchAction)

enum class TwoFingerTwistAction : int {
	Nothing,
	Rotate,
	RotateNoSnap,
	RotateDiscrete,
};
Q_ENUM_NS(TwoFingerTwistAction)

enum class TouchTapAction : int {
	Nothing,
	Undo,
	Redo,
	HideDocks,
	ColorPicker,
	Eraser,
	EraseMode,
	RecolorMode,
};
Q_ENUM_NS(TouchTapAction)

enum class TouchTapAndHoldAction : int {
	Nothing,
	ColorPickMode,
};
Q_ENUM_NS(TouchTapAndHoldAction)

enum class SamplingRingVisibility : int {
	Never,
	TouchOnly,
	Always,
};
Q_ENUM_NS(SamplingRingVisibility)

}

#endif
