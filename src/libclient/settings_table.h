#include "libclient/settings_table_macros.h"

SETTING(_parentalControlsLevelDummy , _ParentalControlsLevelDummy , "_parentalcontrolsleveldummy"           , parentalcontrols::Level::Unrestricted)
SETTING(autoSaveIntervalMinutes     , AutoSaveIntervalMinutes     , "settings/autosaveminutes"              , config::Config::defaultAutoSaveIntervalMinutes())
SETTING(cancelDeselects             , CancelDeselects             , "settings/canceldeselects"              , config::Config::defaultCancelDeselects())
SETTING(checkerColor1               , CheckerColor1               , "settings/checkercolor1"                , config::Config::defaultCheckerColor1())
SETTING(checkerColor2               , CheckerColor2               , "settings/checkercolor2"                , config::Config::defaultCheckerColor2())
SETTING(interpolateInputs           , InterpolateInputs           , "settings/input/interpolate"            , config::Config::defaultInterpolateInputs())
SETTING(messageQueueDrainRate       , MessageQueueDrainRate       , "settings/messagequeuedrainrate"        , config::Config::defaultMessageQueueDrainRate())
SETTING(mouseSmoothing              , MouseSmoothing              , "settings/input/mousesmoothing"         , config::Config::defaultMouseSmoothing())
SETTING(networkProxyMode            , NetworkProxyMode            , "settings/networkproxymode"             , config::Config::defaultNetworkProxyMode())
SETTING(parentalControlsAutoTag     , ParentalControlsAutoTag     , "pc/autotag"                            , config::Config::defaultParentalControlsAutoTag())
SETTING(parentalControlsForceCensor , ParentalControlsForceCensor , "pc/noUncensoring"                      , config::Config::defaultParentalControlsForceCensor())
SETTING_GETSET_V(
    V1, parentalControlsLevel       , ParentalControlsLevel       , "pc/level"                              , config::Config::defaultParentalControlsLevel(),
    &parentalcontrolslevel::get, &any::set)
SETTING(parentalControlsLocked      , ParentalControlsLocked      , "pc/locked"                             , config::Config::defaultParentalControlsLocked())
SETTING(parentalControlsTags        , ParentalControlsTags        , "pc/tagwords"                           , config::Config::defaultParentalControlsTags())
SETTING(engineFrameRate             , EngineFrameRate             , "settings/paintengine/fps"              , config::Config::defaultEngineFrameRate())
SETTING(engineSnapshotCount         , EngineSnapshotCount         , "settings/paintengine/snapshotcount"    , config::Config::defaultEngineSnapshotCount())
SETTING(engineSnapshotInterval      , EngineSnapshotInterval      , "settings/paintengine/snapshotinterval" , config::Config::defaultEngineSnapshotInterval())
SETTING(engineUndoDepth             , EngineUndoDepth             , "settings/paintengine/undodepthlimit"   , config::Config::defaultEngineUndoDepth())
SETTING(listServers                 , ListServers                 , "listservers"                           , config::Config::defaultListServers())
SETTING(selectionColor              , SelectionColor              , "settings/selectioncolor"               , config::Config::defaultSelectionColor())
SETTING(serverAutoReset             , ServerAutoReset             , "settings/server/autoreset"             , config::Config::defaultServerAutoReset())
SETTING(serverPort                  , ServerPort                  , "settings/server/port"                  , config::Config::defaultServerPort())
SETTING(serverTimeout               , ServerTimeout               , "settings/server/timeout"               , config::Config::defaultServerTimeout())
SETTING(smoothing                   , Smoothing                   , "settings/input/smooth"                 , config::Config::defaultSmoothing())

#include "libclient/settings_table_macros.h"
