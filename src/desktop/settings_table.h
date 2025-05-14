#include "libclient/settings_table_macros.h"

#ifndef CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT
#	define CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT QColor(100, 100, 100)
#endif

#ifndef DEBOUNCE_DELAY_MS_DEFAULT
#	define DEBOUNCE_DELAY_MS_DEFAULT 250
#endif

#ifndef GLOBAL_PRESSURE_CURVE_DEFAULT
#	ifdef __EMSCRIPTEN__
#		define GLOBAL_PRESSURE_CURVE_DEFAULT desktop::settings::globalPressureCurveDefault
#	else
#		define GLOBAL_PRESSURE_CURVE_DEFAULT QString("0,0;1,1;")
#endif
#endif

#ifndef INTERFACE_MODE_DEFAULT
#	if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#		define INTERFACE_MODE_DEFAULT InterfaceMode::Dynamic
#	else
#		define INTERFACE_MODE_DEFAULT InterfaceMode::Desktop
#	endif
#endif

#ifndef KINETIC_SCROLL_GESTURE_DEFAULT
#	if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#		define KINETIC_SCROLL_GESTURE_DEFAULT KineticScrollGesture::LeftClick
#	else
#		define KINETIC_SCROLL_GESTURE_DEFAULT KineticScrollGesture::None
#	endif
#endif

#ifndef LAYER_SKETCH_TINT_DEFAULT
#	define LAYER_SKETCH_TINT_DEFAULT QColor(62, 140, 236)
#endif

#ifndef ONE_FINGER_TOUCH_DEFAULT
#	if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#		define ONE_FINGER_TOUCH_DEFAULT desktop::settings::OneFingerTouchAction::Guess
#	else
#		define ONE_FINGER_TOUCH_DEFAULT desktop::settings::OneFingerTouchAction::Pan
#	endif
#endif

#ifndef OVERRIDE_FONT_SIZE_DEFAULT
#	if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#		define OVERRIDE_FONT_SIZE_DEFAULT true
#	else
#		define OVERRIDE_FONT_SIZE_DEFAULT false
#	endif
#endif

#ifndef SCALING_OVERRIDE_DEFAULT
#	if defined(Q_OS_ANDROID)
#		define SCALING_OVERRIDE_DEFAULT true
#	else
#		define SCALING_OVERRIDE_DEFAULT false
#	endif
#endif

#ifndef SESSION_UNDO_LIMIT_DEFAULT
#	define SESSION_UNDO_LIMIT_DEFAULT 60
#endif

#ifndef THEME_PALETTE_DEFAULT
#	define THEME_PALETTE_DEFAULT ThemePalette::KritaDark
#endif

#ifndef THEME_STYLE_DEFAULT
#	define THEME_STYLE_DEFAULT QString("Fusion")
#endif

#ifndef UPDATE_CHECK_DEFAULT
#	if DISABLE_UPDATE_CHECK_DEFAULT
#		define UPDATE_CHECK_DEFAULT false
#	else
#		define UPDATE_CHECK_DEFAULT true
#	endif
#endif

#define COLOR_SWATCH_NO_CIRCLE (1 << 0)
#define COLOR_SWATCH_NO_PALETTE (1 << 1)
#define COLOR_SWATCH_NO_SLIDERS (1 << 2)
#define COLOR_SWATCH_NO_SPINNER (1 << 3)
#define COLOR_SWATCH_NONE                                                      \
	(COLOR_SWATCH_NO_CIRCLE | COLOR_SWATCH_NO_PALETTE |                        \
	 COLOR_SWATCH_NO_SLIDERS | COLOR_SWATCH_NO_SPINNER)

SETTING(_brushCursorDummy         , _BrushCursorDummy         , "_brushcursordummy"                     , widgets::CanvasView::BrushCursor::Dot)
SETTING_GETSET_V(
	V1, alphaLockCursor           , AlphaLockCursor           , "settings/alphalockcursor"              , int(view::Cursor::SameAsBrush),
	&viewCursor::get, &viewCursor::set)
#ifdef Q_OS_ANDROID
SETTING_GETSET_V(
	V1, androidStylusChecked      , AndroidStylusChecked      , "settings/android/styluschecked"        , false,
	any::getExactVersion, &any::set)
#endif
SETTING(animationExportFormat     , AnimationExportFormat     , "animationexport/format"                , int(-1))
SETTING(automaticAlphaPreserve    , AutomaticAlphaPreserve    , "settings/automaticalphapreserve"       , int(1))
SETTING_GETSET_V(
	V1, brushCursor               , BrushCursor               , "settings/brushcursor"                  , int(view::Cursor::TriangleRight),
	&viewCursor::get, &viewCursor::set)
SETTING(brushOutlineWidth         , BrushOutlineWidth         , "settings/brushoutlinewidth"            , 1.0)
SETTING(brushPresetsAttach        , BrushPresetsAttach        , "settings/brushpresetsattach"           , true)
SETTING(brushSlotCount            , BrushSlotCount            , "settings/brushslotcount"               , 5)
SETTING(canvasViewBackgroundColor , CanvasViewBackgroundColor , "settings/canvasviewbackgroundcolor"    , CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT)
SETTING(canvasScrollBars          , CanvasScrollBars          , "settings/canvasscrollbars"             , true)
SETTING(canvasShortcuts           , CanvasShortcuts           , "settings/canvasshortcuts2"             , QVariantMap())
#ifdef Q_OS_ANDROID
SETTING(captureVolumeRocker       , CaptureVolumeRocker       , "settings/android/capturevolumerocker"  , true)
#endif
SETTING(colorCircleGamutMaskAngle , ColorCircleGamutMaskAngle , "settings/colorcircle/gamutmaskangle"   , 0)
SETTING(colorCircleGamutMaskPath  , ColorCircleGamutMaskPath  , "settings/colorcircle/gamutmaskpath"    , QString())
SETTING(colorCircleGamutMaskOpacity,ColorCircleGamutMaskOpacity,"settings/colorcircle/gamutmaskopacity" , 0.75)
SETTING(colorCircleHueAngle       , ColorCircleHueAngle       , "settings/colorcircle/hueangle"         , 0)
SETTING(colorCircleHueCount       , ColorCircleHueCount       , "settings/colorcircle/huecount"         , 16)
SETTING(colorCircleHueLimit       , ColorCircleHueLimit       , "settings/colorcircle/huelimit"         , false)
SETTING(colorCircleSaturationCount, ColorCircleSaturationCount, "settings/colorcircle/saturationcount"  , 8)
SETTING(colorCircleSaturationLimit, ColorCircleSaturationLimit, "settings/colorcircle/saturationlimit"  , false)
SETTING(colorCircleValueCount     , ColorCircleValueCount     , "settings/colorcircle/valuecount"       , 10)
SETTING(colorCircleValueLimit     , ColorCircleValueLimit     , "settings/colorcircle/valuelimit"       , false)
SETTING(colorSlidersShowAll       , ColorSlidersShowAll       , "settings/colorsliders/showall"         , false)
SETTING(colorSlidersShowInput     , ColorSlidersShowInput     , "settings/colorsliders/showinput"       , true)
SETTING(colorSlidersMode          , ColorSlidersMode          , "settings/colorsliders/mode"            , 0)
SETTING(colorSwatchFlags          , ColorSwatchFlags          , "settings/colorswatchflags"             , 0)
SETTING(colorWheelAngle           , ColorWheelAngle           , "settings/colorwheel/rotate"            , color_widgets::ColorWheel::AngleEnum::AngleRotating)
SETTING(colorWheelAlign           , ColorWheelAlign           , "settings/colorwheel/align"             , int(Qt::AlignVCenter))
SETTING(colorWheelMirror          , ColorWheelMirror          , "settings/colorwheel/mirror"            , true)
SETTING(colorWheelPreview         , ColorWheelPreview         , "settings/colorwheel/preview"           , 1)
SETTING(colorWheelShape           , ColorWheelShape           , "settings/colorwheel/shape"             , color_widgets::ColorWheel::ShapeEnum::ShapeTriangle)
SETTING(colorWheelSpace           , ColorWheelSpace           , "settings/colorwheel/space"             , color_widgets::ColorWheel::ColorSpaceEnum::ColorHSV)
SETTING(compactChat               , CompactChat               , "history/compactchat"                   , false)
SETTING(confirmLayerDelete        , ConfirmLayerDelete        , "settings/confirmlayerdelete"           , false)
SETTING_GETSET(debounceDelayMs    , DebounceDelayMs           , "settings/debouncedelayms"              , DEBOUNCE_DELAY_MS_DEFAULT
	, &debounceDelayMs::get, &debounceDelayMs::set)
SETTING(parentalControlsHideLocked, ParentalControlsHideLocked, "pc/hidelocked"                         , false)
SETTING(curvesPresets             , CurvesPresets             , "curves/presets"                        , QVector<QVariantMap>())
SETTING(curvesPresetsConverted    , CurvesPresetsConverted    , "curves/inputpresetsconverted"          , false)
SETTING(doubleTapAltToFocusCanvas , DoubleTapAltToFocusCanvas , "settings/doubletapalttofocuscanvas"    , true)
SETTING_GETSET_V(
	V1, eraseCursor               , EraseCursor               , "settings/erasecursor"                  , int(view::Cursor::SameAsBrush),
	&viewCursor::get, &viewCursor::set)
SETTING(filterClosed              , FilterClosed              , "history/filterclosed"                  , false)
SETTING(filterDuplicates          , FilterDuplicates          , "history/filterduplicates"              , false)
SETTING(filterInactive            , FilterInactive            , "history/filterinactive"                , true)
SETTING(filterLocked              , FilterLocked              , "history/filterlocked"                  , false)
SETTING(filterNsfm                , FilterNsfm                , "history/filternsfw"                    , false)
SETTING(flipbookWindow            , FlipbookWindow            , "flipbook/window"                       , QRect())
SETTING(overrideFontSize          , OverrideFontSize          , "settings/overridefontsize"             , OVERRIDE_FONT_SIZE_DEFAULT)
SETTING(fontSize                  , FontSize                  , "settings/fontSize"                     , -1)
SETTING(globalPressureCurve       , GlobalPressureCurve       , "settings/input/globalcurve"            , GLOBAL_PRESSURE_CURVE_DEFAULT)
SETTING(hostEnableAdvanced        , HostEnableAdvanced        , "history/hostenableadvanced"            , false)
SETTING(ignoreCarrierGradeNat     , IgnoreCarrierGradeNat     , "history/cgnalert"                      , false)
SETTING(inputPresets              , InputPresets              , "inputpresets"                          , QVector<QVariantMap>())
SETTING(insecurePasswordStorage   , InsecurePasswordStorage   , "settings/insecurepasswordstorage"      , false)
SETTING_GETSET_V(
	V1, interfaceMode             , InterfaceMode             , "settings/interfacemode"                , int(INTERFACE_MODE_DEFAULT),
	&any::getExactVersion, &any::set)
SETTING(inviteIncludePassword     , InviteIncludePassword     , "invites/includepassword"               , true)
SETTING(kineticScrollGesture      , KineticScrollGesture      , "settings/kineticscroll/gesture"        , int(KINETIC_SCROLL_GESTURE_DEFAULT))
SETTING(kineticScrollThreshold    , KineticScrollThreshold    , "settings/kineticscroll/threshold"      , 10)
SETTING(kineticScrollHideBars     , KineticScrollHideBars     , "settings/kineticscroll/hidebars"       , false)
SETTING(language                  , Language                  , "settings/language"                     , QString())
SETTING(lastAnnounce              , LastAnnounce              , "history/announce"                      , false)
SETTING(lastAvatar                , LastAvatar                , "history/avatar"                        , QString())
SETTING(lastBrowseSortColumn      , LastBrowseSortColumn      , "history/browsesortcolumn"              , -1)
SETTING(lastBrowseSortDirection   , LastBrowseSortDirection   , "history/browsesortdirection"           , 0)
SETTING(lastFileOpenPath          , LastFileOpenPath          , "window/lastpath"                       , QString())
SETTING(lastFileOpenPaths         , LastFileOpenPaths         , "window/lastpaths"                      , QVariantMap())
SETTING_GETSET(lastHostServer     , LastHostServer            , "history/hostserver"                    , -1,
	&lastHostServer::get, &any::set)
SETTING(lastHostType             , LastHostType              , "history/hosttype"                      , 0)
SETTING(lastIdAlias               , LastIdAlias               , "history/idalias"                       , QString())
SETTING(lastJoinAddress           , LastJoinAddress           , "history/joinaddress"                   , QString())
SETTING(lastKeepChat              , LastKeepChat              , "history/keepchat"                      , false)
SETTING(lastListingUrls           , LastListingUrls           , "history/listingurls"                   , QStringList())
SETTING(lastNsfm                  , LastNsfm                  , "history/nsfm"                          , false)
SETTING(lastPalette               , LastPalette               , "history/lastpalette"                   , 0)
SETTING(lastSessionAuthList       , LastSessionAuthList       , "history/sessionauthlist"               , QByteArray())
SETTING(lastSessionAutomatic      , LastSessionAutomatic      , "history/sessionautomatic"              , true)
SETTING(lastSessionBanList        , LastSessionBanList        , "history/sessionbanlist"                , QByteArray())
SETTING(lastSessionOpPassword     , LastSessionOpPassword     , "history/sessionoppassword"             , QString())
SETTING(lastSessionPassword       , LastSessionPassword       , "history/sessionpassword"               , QString())
SETTING(lastSessionPermissions    , LastSessionPermissions    , "history/sessionpermissions"            , QVariantMap())
SETTING(lastSessionTitle          , LastSessionTitle          , "history/sessiontitle"                  , QString())
SETTING(lastSessionUndoDepth      , LastSessionUndoDepth      , "history/sessionundodepth"              , 60)
SETTING(lastStartDialogPage       , LastStartDialogPage       , "history/laststartdialogpage"           , -1)
SETTING(lastStartDialogSize       , LastStartDialogSize       , "history/laststartdialogsize"           , QSize())
SETTING(lastStartDialogDateTime   , LastStartDialogDateTime   , "history/laststartdialogdatetime"       , QString())
SETTING(lastTool                  , LastTool                  , "tools/tool"                            , tools::Tool::Type::FREEHAND)
SETTING(lastToolBackgroundColor   , LastToolBackgroundColor   , "tools/backgroundcolor"                 , QColor(Qt::white))
SETTING(lastToolColor             , LastToolColor             , "tools/color"                           , QColor(Qt::black))
SETTING(lastUsername              , LastUsername              , "history/username"                      , QString())
SETTING(lastWindowActions         , LastWindowActions         , "window/actions"                        , (QMap<QString, bool>()))
SETTING(lastWindowDocks           , LastWindowDocks           , "window/docks"                          , (QVariantMap()))
SETTING(lastWindowMaximized       , LastWindowMaximized       , "window/maximized"                      , true)
SETTING(lastWindowPosition        , LastWindowPosition        , "window/pos"                            , (QPoint()))
SETTING(lastWindowSize            , LastWindowSize            , "window/size"                           , (QSize(800, 600)))
SETTING(lastWindowState           , LastWindowState           , "window/state"                          , QByteArray())
SETTING(lastWindowViewState       , LastWindowViewState       , "window/viewstate"                      , QByteArray())
SETTING(layerSketchOpacityPercent , LayerSketchOpacityPercent , "layers/sketchopacitypercent"           , 75)
SETTING(layerSketchTint           , LayerSketchTint           , "layers/sketchtint"                     , LAYER_SKETCH_TINT_DEFAULT)
SETTING(layouts                   , Layouts                   , "layouts"                               , QVector<QVariantMap>())
SETTING(mentionEnabled            , MentionEnabled            , "settings/mentions/enabled"             , true)
SETTING(mentionTriggerList        , MentionTriggerList        , "settings/mentions/triggerlist"         , QString())
SETTING(navigatorRealtime         , NavigatorRealtime         , "navigator/realtime"                    , false)
SETTING(navigatorShowCursors      , NavigatorShowCursors      , "navigator/showcursors"                 , true)
SETTING_GETSET(newCanvasBackColor , NewCanvasBackColor        , "history/newcolor"                      , (QColor(Qt::white)),
	&newCanvasBackColor::get, &any::set)
SETTING_GETSET(newCanvasSize      , NewCanvasSize             , "history/newsize"                       , (QSize(2000, 2000)),
	&newCanvasSize::get, &any::set)
SETTING(notifFlashChat            , NotifFlashChat            , "notifflashes/chat"                     , true)
SETTING(notifFlashDisconnect      , NotifFlashDisconnect      , "notifflashes/disconnect"               , true)
SETTING(notifFlashLock            , NotifFlashLock            , "notifflashes/lock"                     , false)
SETTING(notifFlashLogin           , NotifFlashLogin           , "notifflashes/login"                    , false)
SETTING(notifFlashLogout          , NotifFlashLogout          , "notifflashes/logout"                   , false)
SETTING(notifFlashPrivateChat     , NotifFlashPrivateChat     , "notifflashes/privatechat"              , true)
SETTING(notifFlashUnlock          , NotifFlashUnlock          , "notifflashes/unlock"                   , false)
SETTING(notifPopupChat            , NotifPopupChat            , "notifpopups/chat"                      , false)
SETTING(notifPopupDisconnect      , NotifPopupDisconnect      , "notifpopups/disconnect"                , true)
SETTING(notifPopupLock            , NotifPopupLock            , "notifpopups/lock"                      , false)
SETTING(notifPopupLogin           , NotifPopupLogin           , "notifpopups/login"                     , true)
SETTING(notifPopupLogout          , NotifPopupLogout          , "notifpopups/logout"                    , true)
SETTING(notifPopupPrivateChat     , NotifPopupPrivateChat     , "notifpopups/privatechat"               , false)
SETTING(notifPopupUnlock          , NotifPopupUnlock          , "notifpopups/unlock"                    , false)
SETTING(notifSoundChat            , NotifSoundChat            , "notifications/chat"                    , true)
SETTING(notifSoundDisconnect      , NotifSoundDisconnect      , "notifications/disconnect"              , true)
SETTING(notifSoundLock            , NotifSoundLock            , "notifications/lock"                    , true)
SETTING(notifSoundLogin           , NotifSoundLogin           , "notifications/login"                   , true)
SETTING(notifSoundLogout          , NotifSoundLogout          , "notifications/logout"                  , true)
SETTING(notifSoundPrivateChat     , NotifSoundPrivateChat     , "notifications/privatechat"             , true)
SETTING(notifSoundUnlock          , NotifSoundUnlock          , "notifications/unlock"                  , true)
SETTING_GETSET(oneFingerTouch     , OneFingerTouch            , "settings/input/onefingertouch"         , int(ONE_FINGER_TOUCH_DEFAULT)
	, &oneFingerTouch::get, &any::set)
SETTING_GETSET(twoFingerPinch     , TwoFingerPinch            , "settings/input/twofingerpinch"         , int(TwoFingerPinchAction::Zoom)
	, &twoFingerPinch::get, &any::set)
SETTING_GETSET(twoFingerTwist     , TwoFingerTwist            , "settings/input/twofingertwist"         , int(TwoFingerTwistAction::Rotate)
	, &twoFingerTwist::get, &any::set)
SETTING(oneFingerTap              , OneFingerTap              , "settings/input/onefingertap"           , int(TouchTapAction::Nothing))
SETTING(twoFingerTap              , TwoFingerTap              , "settings/input/twofingertap"           , int(TouchTapAction::Undo))
SETTING(threeFingerTap            , ThreeFingerTap            , "settings/input/threefingertap"         , int(TouchTapAction::Redo))
SETTING(fourFingerTap             , FourFingerTap             , "settings/input/fourfingertap"          , int(TouchTapAction::HideDocks))
SETTING(oneFingerTapAndHold       , OneFingerTapAndHold       , "settings/input/onefingertapandhold"    , int(TouchTapAndHoldAction::ColorPickMode))
SETTING(tabletPressTimerDelay     , TabletPressTimerDelay     , "settings/input/tabletpresstimerdelay"  , 500)
SETTING(touchGestures             , TouchGestures             , "settings/input/touchgestures"          , false)
SETTING(onionSkinsFrameCount      , OnionSkinsFrameCount      , "onionskins/framecount"                 , 8)
SETTING_GETSET_V(
	V1, onionSkinsFrames          , OnionSkinsFrames          , "onionskins/frames"                     , (QMap<int, int>()),
	&onionSkinsFrames::get, &onionSkinsFrames::set)
SETTING(onionSkinsTintAbove       , OnionSkinsTintAbove       , "onionskins/tintabove"                  , (QColor::fromRgb(0x33, 0x33, 0xff, 0x80)))
SETTING(onionSkinsTintBelow       , OnionSkinsTintBelow       , "onionskins/tintbelow"                  , (QColor::fromRgb(0xff, 0x33, 0x33, 0x80)))
SETTING(onionSkinsWrap            , OnionSkinsWrap            , "onionskins/wrap"                       , true)
SETTING(promptLayerCreate         , PromptLayerCreate         , "settings/promptlayercreate"            , false)
SETTING(preferredExportFormat     , PreferredExportFormat     , "settings/preferredexportformat"        , QString())
SETTING(preferredSaveFormat       , PreferredSaveFormat       , "settings/preferredsaveformat"          , QString())
SETTING(recentFiles               , RecentFiles               , "history/recentfiles"                   , QStringList())
SETTING(recentHosts               , RecentHosts               , "history/recenthosts"                   , QStringList())
SETTING(recentRemoteHosts         , RecentRemoteHosts         , "history/recentremotehosts"             , QStringList())
SETTING(renderCanvas              , RenderCanvas              , "settings/render/canvas"                , int(libclient::settings::CanvasImplementation::Default))
SETTING(renderSmooth              , RenderSmooth              , "settings/render/smooth"                , true)
SETTING(renderUpdateFull          , RenderUpdateFull          , "settings/render/updatefull"            , false)
SETTING(samplingRingVisibility    , SamplingRingVisibility    , "settings/colorpicker/samplingring"     , int(SamplingRingVisibility::Always))
SETTING(serverHideIp              , ServerHideIp              , "settings/hideServerIp"                 , false)
SETTING(shareBrushSlotColor       , ShareBrushSlotColor       , "settings/sharebrushslotcolor"          , false)
SETTING(shortcuts                 , Shortcuts                 , "settings/shortcuts"                    , QVariantMap())
SETTING(showInviteDialogOnHost    , ShowInviteDialogOnHost    , "invites/showdialogonhost"              , true)
SETTING(showNsfmWarningOnJoin     , ShowNsfmWarningOnJoin     , "pc/shownsfmwarningonjoin"              , true)
SETTING(showTransformNotices      , ShowTransformNotices      , "settings/showtransformnotices"         , true)
SETTING(showTrayIcon              , ShowTrayIcon              , "ui/trayicon"                           , true)
SETTING(soundVolume               , SoundVolume               , "notifications/volume"                  , SESSION_UNDO_LIMIT_DEFAULT)
SETTING_GETSET(tabletDriver       , TabletDriver              , "settings/input/tabletdriver"           , tabletinput::Mode::KisTabletWinink
	, &tabletDriver::get, &tabletDriver::set)
SETTING_GETSET(tabletEraserAction , TabletEraserAction        , "settings/input/tableteraseraction"     , int(tabletinput::EraserAction::Default)
	, &tabletEraserAction::get, &tabletEraserAction::set)
SETTING(tabletEvents              , TabletEvents              , "settings/input/tabletevents"           , true)
SETTING(temporaryToolSwitch       , TemporaryToolSwitch       , "settings/tools/temporaryswitch"        , true)
SETTING(temporaryToolSwitchMs     , TemporaryToolSwitchMs     , "settings/tools/temporaryswitchms"      , 250)
SETTING_GETSET_V(V3, themePalette , ThemePalette              , "settings/theme/palette"                , THEME_PALETTE_DEFAULT
	, &any::get	      , &themePalette::set)
SETTING_FULL(V2, themeStyle       , ThemeStyle                , "settings/theme/style"                  , THEME_STYLE_DEFAULT
	, &any::get       , &any::set, &themeStyle::notify)
SETTING(toolToggle                , ToolToggle                , "settings/tooltoggle"                   , true)
SETTING(toolset                   , Toolset                   , "tools/toolset"                         , (QMap<QString, QVariantHash>()))
SETTING(touchDrawPressure         , TouchDrawPressure         , "settings/input/touchdrawpressure"      , false)
SETTING(updateCheckEnabled        , UpdateCheckEnabled        , "settings/updatecheck"                  , UPDATE_CHECK_DEFAULT)
SETTING(userMarkerPersistence     , UserMarkerPersistence     , "settings/usermarkerpersistence"        , 1000)
SETTING(videoExportCustomFfmpeg   , VideoExportCustomFfmpeg   , "videoexport/customffmpeg"              , QString())
SETTING(videoExportFfmpegPath     , VideoExportFfmpegPath     , "videoexport/ffmpegpath"                , QString("ffmpeg"))
SETTING(videoExportFormat         , VideoExportFormat         , "videoexport/formatchoice"              , VideoExporter::Format::IMAGE_SERIES)
SETTING(videoExportFrameHeight    , VideoExportFrameHeight    , "videoexport/frameheight"               , 720)
SETTING(videoExportFrameRate      , VideoExportFrameRate      , "videoexport/fps"                       , 30)
SETTING(videoExportFrameWidth     , VideoExportFrameWidth     , "videoexport/framewidth"                , 1280)
SETTING(videoExportSizeChoice     , VideoExportSizeChoice     , "videoexport/sizeChoice"                , 0) // TODO: Enum
SETTING(welcomePageShown          , WelcomePageShown          , "history/welcomepageshown"              , false)
SETTING(writeLogFile              , WriteLogFile              , "settings/logfile"                      , false)

#include "libclient/settings_table_macros.h"
