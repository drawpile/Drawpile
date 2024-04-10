#include "libclient/settings_table_macros.h"

#define CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT QColor(100, 100, 100)

#define DEBOUNCE_DELAY_MS_DEFAULT 250

#ifndef GLOBAL_PRESSURE_CURVE_DEFAULT
#	ifdef __EMSCRIPTEN__
#		define GLOBAL_PRESSURE_CURVE_DEFAULT desktop::settings::globalPressureCurveDefault
#	else
#		define GLOBAL_PRESSURE_CURVE_DEFAULT QString("0,0;1,1;")
#endif
#endif

#ifndef KINETIC_SCROLL_GESTURE_DEFAULT
#	ifdef Q_OS_ANDROID
#		define KINETIC_SCROLL_GESTURE_DEFAULT KineticScrollGesture::LeftClick
#	else
#		define KINETIC_SCROLL_GESTURE_DEFAULT KineticScrollGesture::None
#	endif
#endif

#ifndef OVERRIDE_FONT_SIZE_DEFAULT
#	if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#		define OVERRIDE_FONT_SIZE_DEFAULT true
#	else
#		define OVERRIDE_FONT_SIZE_DEFAULT false
#	endif
#endif

#ifndef THEME_PALETTE_DEFAULT
#	ifdef Q_OS_MACOS
#		define THEME_PALETTE_DEFAULT ThemePalette::System
#	else
#		define THEME_PALETTE_DEFAULT ThemePalette::KritaDarker
#	endif
#endif

#ifndef THEME_STYLE_DEFAULT
#	ifdef Q_OS_MACOS
#		define THEME_STYLE_DEFAULT QString()
#	else
#		define THEME_STYLE_DEFAULT QString("Fusion")
#	endif
#endif

SETTING(_brushCursorDummy         , _BrushCursorDummy         , "_brushcursordummy"                     , widgets::CanvasView::BrushCursor::Dot)
SETTING_GETSET_V(
	V1, alphaLockCursor           , AlphaLockCursor           , "settings/alphalockcursor"              , int(view::Cursor::SameAsBrush),
	&viewCursor::get, &viewCursor::set)
#ifdef Q_OS_ANDROID
SETTING(androidStylusChecked      , AndroidStylusChecked      , "settings/android/styluschecked"        , false)
#endif
SETTING_GETSET_V(
	V1, brushCursor               , BrushCursor               , "settings/brushcursor"                  , int(view::Cursor::TriangleRight),
	&viewCursor::get, &viewCursor::set)
SETTING(brushOutlineWidth         , BrushOutlineWidth         , "settings/brushoutlinewidth"            , 1.0)
SETTING(canvasViewBackgroundColor , CanvasViewBackgroundColor , "settings/canvasviewbackgroundcolor"    , CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT)
SETTING(canvasScrollBars          , CanvasScrollBars          , "settings/canvasscrollbars"             , true)
SETTING(canvasShortcuts           , CanvasShortcuts           , "settings/canvasshortcuts2"             , QVariantMap())
#ifdef Q_OS_ANDROID
SETTING(captureVolumeRocker       , CaptureVolumeRocker       , "settings/android/capturevolumerocker"  , true)
#endif
SETTING(colorWheelAngle           , ColorWheelAngle           , "settings/colorwheel/rotate"            , color_widgets::ColorWheel::AngleEnum::AngleRotating)
SETTING(colorWheelMirror          , ColorWheelMirror          , "settings/colorwheel/mirror"            , true)
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
SETTING(ignoreZeroPressureInputs  , IgnoreZeroPressureInputs  , "settings/ignorezeropressureinputs"     , true)
SETTING(inputPresets              , InputPresets              , "inputpresets"                          , QVector<QVariantMap>())
SETTING(insecurePasswordStorage   , InsecurePasswordStorage   , "settings/insecurepasswordstorage"      , false)
SETTING(interfaceMode             , InterfaceMode             , "settings/interfacemode"                , int(desktop::settings::InterfaceMode::Unknown))
SETTING(inviteLinkType            , InviteLinkType            , "invites/linktype"                      , int(dialogs::InviteDialog::LinkType::Web))
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
SETTING(lastHostRemote            , LastHostRemote            , "history/hostremote"                    , true)
SETTING(lastIdAlias               , LastIdAlias               , "history/idalias"                       , QString())
SETTING(lastJoinAddress           , LastJoinAddress           , "history/joinaddress"                   , QString())
SETTING(lastListingServer         , LastListingServer         , "history/listingserver"                 , -1)
SETTING(lastNsfm                  , LastNsfm                  , "history/nsfm"                          , false)
SETTING(lastPalette               , LastPalette               , "history/lastpalette"                   , 0)
SETTING(lastSessionTitle          , LastSessionTitle          , "history/sessiontitle"                  , QString())
SETTING(lastSessionPassword       , LastSessionPassword       , "history/sessionpassword"               , QString())
SETTING(lastStartDialogPage       , LastStartDialogPage       , "history/laststartdialogpage"           , -1)
SETTING(lastStartDialogSize       , LastStartDialogSize       , "history/laststartdialogsize"           , QSize())
SETTING(lastStartDialogDateTime   , LastStartDialogDateTime   , "history/laststartdialogdatetime"       , QString())
SETTING(lastTool                  , LastTool                  , "tools/tool"                            , tools::Tool::Type::FREEHAND)
SETTING(lastToolColor             , LastToolColor             , "tools/color"                           , QColor(Qt::black))
SETTING(lastUsername              , LastUsername              , "history/username"                      , QString())
SETTING(lastWindowActions         , LastWindowActions         , "window/actions"                        , (QMap<QString, bool>()))
SETTING(lastWindowDocks           , LastWindowDocks           , "window/docks"                          , (QVariantMap()))
SETTING(lastWindowMaximized       , LastWindowMaximized       , "window/maximized"                      , true)
SETTING(lastWindowPosition        , LastWindowPosition        , "window/pos"                            , (QPoint()))
SETTING(lastWindowSize            , LastWindowSize            , "window/size"                           , (QSize(800, 600)))
SETTING(lastWindowState           , LastWindowState           , "window/state"                          , QByteArray())
SETTING(lastWindowViewState       , LastWindowViewState       , "window/viewstate"                      , QByteArray())
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
SETTING(oneFingerDraw             , OneFingerDraw             , "settings/input/touchdraw"              , false)
SETTING(oneFingerScroll           , OneFingerScroll           , "settings/input/touchscroll"            , true)
SETTING(touchGestures             , TouchGestures             , "settings/input/touchgestures"          , false)
SETTING(onionSkinsFrameCount      , OnionSkinsFrameCount      , "onionskins/framecount"                 , 8)
SETTING_GETSET_V(
	V1, onionSkinsFrames          , OnionSkinsFrames          , "onionskins/frames"                     , (QMap<int, int>()),
	&onionSkinsFrames::get, &onionSkinsFrames::set)
SETTING(onionSkinsTintAbove       , OnionSkinsTintAbove       , "onionskins/tintabove"                  , (QColor::fromRgb(0x33, 0x33, 0xff, 0x80)))
SETTING(onionSkinsTintBelow       , OnionSkinsTintBelow       , "onionskins/tintbelow"                  , (QColor::fromRgb(0xff, 0x33, 0x33, 0x80)))
SETTING(onionSkinsWrap            , OnionSkinsWrap            , "onionskins/wrap"                       , true)
SETTING(promptLayerCreate         , PromptLayerCreate         , "settings/promptlayercreate"            , false)
SETTING(recentFiles               , RecentFiles               , "history/recentfiles"                   , QStringList())
SETTING(recentHosts               , RecentHosts               , "history/recenthosts"                   , QStringList())
SETTING(recentRemoteHosts         , RecentRemoteHosts         , "history/recentremotehosts"             , QStringList())
SETTING(renderCanvas              , RenderCanvas              , "settings/render/canvas"                , int(libclient::settings::CanvasImplementation::Default))
SETTING(renderSmooth              , RenderSmooth              , "settings/render/smooth"                , true)
SETTING(renderUpdateFull          , RenderUpdateFull          , "settings/render/updatefull"            , false)
SETTING(serverHideIp              , ServerHideIp              , "settings/hideServerIp"                 , false)
SETTING(shareBrushSlotColor       , ShareBrushSlotColor       , "settings/sharebrushslotcolor"          , false)
SETTING(shortcuts                 , Shortcuts                 , "settings/shortcuts"                    , QVariantMap())
SETTING(showInviteDialogOnHost    , ShowInviteDialogOnHost    , "invites/showdialogonhost"              , true)
SETTING(showNsfmWarningOnJoin     , ShowNsfmWarningOnJoin     , "pc/shownsfmwarningonjoin"              , true)
SETTING(showTransformNotices      , ShowTransformNotices      , "settings/showtransformnotices"         , true)
SETTING(showTrayIcon              , ShowTrayIcon              , "ui/trayicon"                           , true)
SETTING(soundVolume               , SoundVolume               , "notifications/volume"                  , 60)
SETTING_GETSET(tabletDriver       , TabletDriver              , "settings/input/tabletdriver"           , tabletinput::Mode::KisTabletWinink
	, &tabletDriver::get, &tabletDriver::set)
SETTING_GETSET(tabletEraserAction , TabletEraserAction        , "settings/input/tableteraseraction"     , int(tabletinput::EraserAction::Default)
	, &tabletEraserAction::get, &tabletEraserAction::set)
SETTING(tabletEvents              , TabletEvents              , "settings/input/tabletevents"           , true)
SETTING_GETSET_V(V2, themePalette , ThemePalette              , "settings/theme/palette"                , THEME_PALETTE_DEFAULT
	, &any::get	      , &any::set)
SETTING_FULL(V2, themeStyle       , ThemeStyle                , "settings/theme/style"                  , THEME_STYLE_DEFAULT
	, &any::get       , &any::set, &themeStyle::notify)
SETTING(toolToggle                , ToolToggle                , "settings/tooltoggle"                   , true)
SETTING(toolset                   , Toolset                   , "tools/toolset"                         , (QMap<QString, QVariantHash>()))
SETTING(twoFingerRotate           , TwoFingerRotate           , "settings/input/touchtwist"             , true)
SETTING(twoFingerZoom             , TwoFingerZoom             , "settings/input/touchpinch"             , true)
SETTING(updateCheckEnabled        , UpdateCheckEnabled        , "settings/updatecheck"                  , true)
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
