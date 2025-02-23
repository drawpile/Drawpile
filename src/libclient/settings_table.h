#include "libclient/settings_table_macros.h"

#ifndef CHECKER_COLOR1_DEFAULT
#   define CHECKER_COLOR1_DEFAULT QColor(100, 100, 100)
#endif

#ifndef CHECKER_COLOR2_DEFAULT
#   define CHECKER_COLOR2_DEFAULT QColor(135, 135, 135)
#endif

#ifndef ENGINE_UNDO_LIMIT_DEFAULT
#	define ENGINE_UNDO_LIMIT_DEFAULT 60
#endif

#ifndef SNAPSHOT_COUNT_DEFAULT
#   if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#       define SNAPSHOT_COUNT_DEFAULT 0
#   else
#       define SNAPSHOT_COUNT_DEFAULT 5
#   endif
#endif

SETTING(autoSaveIntervalMinutes     , AutoSaveIntervalMinutes     , "settings/autosaveminutes"              , 5)
SETTING(checkerColor1               , CheckerColor1               , "settings/checkercolor1"                , CHECKER_COLOR1_DEFAULT)
SETTING(checkerColor2               , CheckerColor2               , "settings/checkercolor2"                , CHECKER_COLOR2_DEFAULT)
SETTING(interpolateInputs           , InterpolateInputs           , "settings/input/interpolate"            , true)
SETTING(messageQueueDrainRate       , MessageQueueDrainRate       , "settings/messagequeuedrainrate"        , net::MessageQueue::DEFAULT_SMOOTH_DRAIN_RATE)
SETTING(mouseSmoothing              , MouseSmoothing              , "settings/input/mousesmoothing"         , false)
SETTING(networkProxyMode            , NetworkProxyMode            , "settings/networkproxymode"             , 0)
SETTING(parentalControlsAutoTag     , ParentalControlsAutoTag     , "pc/autotag"                            , true)
SETTING(parentalControlsForceCensor , ParentalControlsForceCensor , "pc/noUncensoring"                      , false)
SETTING(parentalControlsLevel       , ParentalControlsLevel       , "pc/level"                              , parentalcontrols::Level::Unrestricted)
SETTING(parentalControlsLocked      , ParentalControlsLocked      , "pc/locked"                             , QByteArray())
SETTING(parentalControlsTags        , ParentalControlsTags        , "pc/tagwords"                           , parentalcontrols::defaultWordList())
SETTING(engineFrameRate             , EngineFrameRate             , "settings/paintengine/fps"              , 60)
SETTING(engineSnapshotCount         , EngineSnapshotCount         , "settings/paintengine/snapshotcount"    , SNAPSHOT_COUNT_DEFAULT)
SETTING(engineSnapshotInterval      , EngineSnapshotInterval      , "settings/paintengine/snapshotinterval" , 10)
SETTING(engineUndoDepth             , EngineUndoDepth             , "settings/paintengine/undodepthlimit"   , ENGINE_UNDO_LIMIT_DEFAULT)
SETTING(listServers                 , ListServers                 , "listservers"                           , QVector<QVariantMap>())
SETTING(serverAutoReset             , ServerAutoReset             , "settings/server/autoreset"             , true)
SETTING(serverPort                  , ServerPort                  , "settings/server/port"                  , cmake_config::proto::port())
SETTING(serverTimeout               , ServerTimeout               , "settings/server/timeout"               , 60)
SETTING(smoothing                   , Smoothing                   , "settings/input/smooth"                 , defaultSmoothing)

#include "libclient/settings_table_macros.h"
