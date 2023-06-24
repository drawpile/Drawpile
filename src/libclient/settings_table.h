#include "libclient/settings_table_macros.h"

SETTING(autoSaveInterval            , AutoSaveInterval            , "settings/autosave"                     , 5000)
SETTING(messageQueueDrainRate       , MessageQueueDrainRate       , "settings/messagequeuedrainrate"        , defaultMessageQueueDrainRate)
SETTING(parentalControlsAutoTag     , ParentalControlsAutoTag     , "pc/autotag"                            , true)
SETTING(parentalControlsForceCensor , ParentalControlsForceCensor , "pc/noUncensoring"                      , false)
SETTING(parentalControlsLevel       , ParentalControlsLevel       , "pc/level"                              , parentalcontrols::Level::Unrestricted)
SETTING(parentalControlsLocked      , ParentalControlsLocked      , "pc/locked"                             , QByteArray())
SETTING(parentalControlsTags        , ParentalControlsTags        , "pc/tagwords"                           , parentalcontrols::defaultWordList())
SETTING(engineFrameRate             , EngineFrameRate             , "settings/paintengine/fps"              , 60)
SETTING(engineSnapshotCount         , EngineSnapshotCount         , "settings/paintengine/snapshotcount"    , 5)
SETTING(engineSnapshotInterval      , EngineSnapshotInterval      , "settings/paintengine/snapshotinterval" , 10)
SETTING(engineUndoDepth             , EngineUndoDepth             , "settings/paintengine/undodepthlimit"   , DP_UNDO_DEPTH_DEFAULT)
SETTING(listServers                 , ListServers                 , "listservers"                           , QVector<QVariantMap>())
SETTING(serverAutoReset             , ServerAutoReset             , "settings/server/autoreset"             , true)
SETTING(serverTimeout               , ServerTimeout               , "settings/server/timeout"               , 60)
SETTING(smoothing                   , Smoothing                   , "settings/input/smooth"                 , defaultSmoothing)
SETTING(versionCheckEnabled         , VersionCheckEnabled         , "versioncheck/enabled"                  , false)
SETTING(versionCheckLastCheck       , VersionCheckLastCheck       , "versioncheck/lastcheck"                , QString())
SETTING(versionCheckLastSuccess     , VersionCheckLastSuccess     , "versioncheck/lastsuccess"              , false)
SETTING(versionCheckLatest          , VersionCheckLatest          , "versioncheck/latest"                   , QString())

#include "libclient/settings_table_macros.h"
