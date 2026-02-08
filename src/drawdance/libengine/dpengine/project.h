// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_H
#define DPENGINE_PROJECT_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_LocalStateAction DP_LocalStateAction;
typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;
typedef struct DP_Rect DP_Rect;


#define DP_PROJECT_APPLICATION_ID 520585024
#define DP_PROJECT_USER_VERSION   1

#define DP_PROJECT_CANVAS_APPLICATION_ID 520585025
#define DP_PROJECT_CANVAS_USER_VERSION   1

#define DP_PROJECT_THUMBNAIL_SIZE 256

#define DP_PROJECT_ERROR_IN(VALUE, CATEGORY) \
    (VALUE <= CATEGORY##_UNKNOWN && VALUE > CATEGORY##_UNKNOWN - 100)

#define DP_PROJECT_CHECK_ERROR_UNKNOWN        (-100)
#define DP_PROJECT_CHECK_ERROR_OPEN           (-101)
#define DP_PROJECT_CHECK_ERROR_READ           (-102)
#define DP_PROJECT_CHECK_ERROR_HEADER         (-103)
#define DP_PROJECT_CHECK_ERROR_MAGIC          (-104)
#define DP_PROJECT_CHECK_ERROR_APPLICATION_ID (-105)
#define DP_PROJECT_CHECK_ERROR_USER_VERSION   (-106)

#define DP_PROJECT_OPEN_ERROR_UNKNOWN         (-200)
#define DP_PROJECT_OPEN_ERROR_INIT            (-202)
#define DP_PROJECT_OPEN_ERROR_OPEN            (-203)
#define DP_PROJECT_OPEN_ERROR_READ_ONLY       (-204)
#define DP_PROJECT_OPEN_ERROR_SETUP           (-205)
#define DP_PROJECT_OPEN_ERROR_READ_EMPTY      (-206)
#define DP_PROJECT_OPEN_ERROR_HEADER_WRITE    (-207)
#define DP_PROJECT_OPEN_ERROR_HEADER_READ     (-208)
#define DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH (-209)
#define DP_PROJECT_OPEN_ERROR_MIGRATION       (-210)
#define DP_PROJECT_OPEN_ERROR_PREPARE         (-211)
#define DP_PROJECT_OPEN_ERROR_LOCKED          (-212)

#define DP_PROJECT_SESSION_OPEN_ERROR_UNKNOWN      (-300)
#define DP_PROJECT_SESSION_OPEN_ERROR_MISUSE       (-301)
#define DP_PROJECT_SESSION_OPEN_ERROR_ALREADY_OPEN (-302)
#define DP_PROJECT_SESSION_OPEN_ERROR_PREPARE      (-303)
#define DP_PROJECT_SESSION_OPEN_ERROR_WRITE        (-304)

#define DP_PROJECT_SESSION_CLOSE_ERROR_UNKNOWN   (-400)
#define DP_PROJECT_SESSION_CLOSE_ERROR_MISUSE    (-401)
#define DP_PROJECT_SESSION_CLOSE_ERROR_PREPARE   (-402)
#define DP_PROJECT_SESSION_CLOSE_ERROR_WRITE     (-403)
#define DP_PROJECT_SESSION_CLOSE_ERROR_NO_CHANGE (-404)
#define DP_PROJECT_SESSION_CLOSE_NOT_OPEN        1

#define DP_PROJECT_MESSAGE_RECORD_ERROR_UNKNOWN   (-500)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE    (-501)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_NOT_OPEN  (-502)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_SERIALIZE (-503)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE     (-504)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_PREPARE   (-505)

#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_UNKNOWN      (-600)
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_NO_SESSION   (-601)
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_ALREADY_OPEN (-602)
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_MUTEX        (-603)
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_PREPARE      (-604)
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_WRITE        (-605)

#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_UNKNOWN   (-700)
#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN  (-701)
#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_PREPARE   (-702)
#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_WRITE     (-703)
#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_NO_CHANGE (-704)

#define DP_PROJECT_SNAPSHOT_DISCARD_ERROR_UNKNOWN (-800)
#define DP_PROJECT_SNAPSHOT_DISCARD_ERROR_PREPARE (-801)
#define DP_PROJECT_SNAPSHOT_DISCARD_ERROR_WRITE   (-802)
#define DP_PROJECT_SNAPSHOT_DISCARD_NOT_FOUND     1

#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_UNKNOWN    (-900)
#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_PREPARE    (-901)
#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ       (-902)
#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_WRITE      (-903)
#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ_WRITE (-904)

#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_UNKNOWN   (-1000)
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_MISUSE    (-1001)
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN  (-1002)
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_READY (-1003)
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_WRITE     (-1004)

#define DP_PROJECT_CANVAS_SAVE_ERROR_UNKNOWN       (-1100)
#define DP_PROJECT_CANVAS_SAVE_ERROR_OPEN_SNAPSHOT (-1101)
#define DP_PROJECT_CANVAS_SAVE_ERROR_CLOSE_PROJECT (-1102)

#define DP_PROJECT_CANVAS_LOAD_ERROR_UNKNOWN (-1200)
#define DP_PROJECT_CANVAS_LOAD_ERROR_READ    (-1201)

#define DP_PROJECT_CANVAS_LOAD_WARN_UNKNOWN               (-1300)
#define DP_PROJECT_CANVAS_LOAD_WARN_INCOMPATIBLE          (-1301)
#define DP_PROJECT_CANVAS_LOAD_WARN_MINOR_INCOMPATIBILITY (-1302)
#define DP_PROJECT_CANVAS_LOAD_WARN_PREPARE_ERROR         (-1303)
#define DP_PROJECT_CANVAS_LOAD_WARN_HISTORY_ERROR         (-1304)
#define DP_PROJECT_CANVAS_LOAD_WARN_QUERY_ERROR           (-1305)
#define DP_PROJECT_CANVAS_LOAD_WARN_PLAYBACK_ERROR        (-1306)

#define DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_UNKNOWN  (-1400)
#define DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_MISUSE   (-1401)
#define DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_NOT_OPEN (-1402)
#define DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_PREPARE  (-1403)
#define DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_QUERY    (-1404)

#define DP_PROJECT_SAVE_ERROR_UNKNOWN         (-1500)
#define DP_PROJECT_SAVE_ERROR_MISUSE          (-1501)
#define DP_PROJECT_SAVE_ERROR_NO_SESSION      (-1502)
#define DP_PROJECT_SAVE_ERROR_OPEN            (-1503)
#define DP_PROJECT_SAVE_ERROR_READ_ONLY       (-1504)
#define DP_PROJECT_SAVE_ERROR_SETUP           (-1505)
#define DP_PROJECT_SAVE_ERROR_READ_EMPTY      (-1506)
#define DP_PROJECT_SAVE_ERROR_HEADER_WRITE    (-1507)
#define DP_PROJECT_SAVE_ERROR_HEADER_READ     (-1508)
#define DP_PROJECT_SAVE_ERROR_HEADER_MISMATCH (-1509)
#define DP_PROJECT_SAVE_ERROR_MIGRATION       (-1510)
#define DP_PROJECT_SAVE_ERROR_PREPARE         (-1511)
#define DP_PROJECT_SAVE_ERROR_LOCKED          (-1512)
#define DP_PROJECT_SAVE_ERROR_QUERY           (-1513)
#define DP_PROJECT_SAVE_ERROR_WRITE           (-1514)

#define DP_PROJECT_INFO_ERROR_UNKNOWN   (-1600)
#define DP_PROJECT_INFO_ERROR_MISUSE    (-1601)
#define DP_PROJECT_INFO_ERROR_PREPARE   (-1602)
#define DP_PROJECT_INFO_ERROR_QUERY     (-1603)
#define DP_PROJECT_INFO_ERROR_CANCELLED (-1604)

#define DP_PROJECT_PLAYBACK_ERROR_UNKNOWN (-1600)
#define DP_PROJECT_PLAYBACK_ERROR_MISUSE  (-1601)
#define DP_PROJECT_PLAYBACK_ERROR_PREPARE (-1602)
#define DP_PROJECT_PLAYBACK_ERROR_QUERY   (-1603)
#define DP_PROJECT_PLAYBACK_ERROR_EMPTY   (-1604)

#define DP_PROJECT_OPEN_EXISTING  (1u << 0u)
#define DP_PROJECT_OPEN_TRUNCATE  (1u << 1u)
#define DP_PROJECT_OPEN_READ_ONLY (1u << 2u)

#define DP_PROJECT_VERIFY_QUICK 0u
#define DP_PROJECT_VERIFY_FULL  (1u << 0u)

#define DP_PROJECT_SOURCE_BLANK   1
#define DP_PROJECT_SOURCE_FILE    2
#define DP_PROJECT_SOURCE_SESSION 3

#define DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED (1u << 0u)

#define DP_PROJECT_MESSAGE_FLAG_OWN      (1u << 0u)
#define DP_PROJECT_MESSAGE_FLAG_CONTINUE (1u << 1u)

#define DP_PROJECT_MESSAGE_INTERNAL_TYPE_RESET (-1)

#define DP_PROJECT_SNAPSHOT_FLAG_COMPLETE       (1u << 0u)
#define DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT     (1u << 1u)
#define DP_PROJECT_SNAPSHOT_FLAG_CANVAS         (1u << 2u)
#define DP_PROJECT_SNAPSHOT_FLAG_AUTOSAVE       (1u << 3u)
#define DP_PROJECT_SNAPSHOT_FLAG_HAS_MESSAGES   (1u << 4u)
#define DP_PROJECT_SNAPSHOT_FLAG_HAS_SUBLAYERS  (1u << 5u)
#define DP_PROJECT_SNAPSHOT_FLAG_HAS_SELECTIONS (1u << 6u)
#define DP_PROJECT_SNAPSHOT_FLAG_NULL_CANVAS    (1u << 7u)
#define DP_PROJECT_SNAPSHOT_FLAG_CONTINUATION   (1u << 8u)

#define DP_PROJECT_SAVE_FLAG_NO_MESSAGES (1u << 0u)

#define DP_PROJECT_INFO_FLAG_HEADER     (1u << 0u)
#define DP_PROJECT_INFO_FLAG_SESSIONS   (1u << 1u)
#define DP_PROJECT_INFO_FLAG_SNAPSHOTS  (1u << 2u)
#define DP_PROJECT_INFO_FLAG_OVERVIEW   (1u << 3u)
#define DP_PROJECT_INFO_FLAG_WORK_TIMES (1u << 4u)

#define DP_PROJECT_PLAYBACK_FLAG_MEASURE_OWN_ONLY (1u << 0u)


typedef struct DP_Project DP_Project;
typedef struct DP_ProjectPlayback DP_ProjectPlayback;

typedef enum DP_ProjectCheckType {
    DP_PROJECT_CHECK_NONE,    // Not a project file.
    DP_PROJECT_CHECK_PROJECT, // Full project file.
    DP_PROJECT_CHECK_CANVAS,  // Single snapshot file.
} DP_ProjectCheckType;

typedef enum DP_ProjectAppendStatus {
    DP_PROJECT_APPEND_STATUS_ERROR,
    DP_PROJECT_APPEND_STATUS_OVERWRITE,
    DP_PROJECT_APPEND_STATUS_APPEND,
} DP_ProjectAppendStatus;

typedef struct DP_ProjectCheckResult {
    DP_ProjectCheckType result;
    union {
        int version;
        int error;
    };
} DP_ProjectCheckResult;

typedef struct DP_ProjectOpenResult {
    DP_Project *project;
    int error;
    int sql_result;
} DP_ProjectOpenResult;

typedef struct DP_ProjectCanvasLoad {
    int result;
    DP_CanvasState *cs;
    char *session_source_param;
    long long session_sequence_id;
} DP_ProjectCanvasLoad;

typedef enum DP_ProjectVerifyStatus {
    DP_PROJECT_VERIFY_OK,
    DP_PROJECT_VERIFY_CORRUPTED,
    DP_PROJECT_VERIFY_ERROR,
    DP_PROJECT_VERIFY_CANCELLED,
} DP_ProjectVerifyStatus;

typedef struct DP_ProjectSessionTimes {
    long long own_work_minutes;
    long long last_sequence_id;
    double last_recorded_at;
} DP_ProjectSessionTimes;

typedef enum DP_ProjectInfoType {
    DP_PROJECT_INFO_TYPE_HEADER,
    DP_PROJECT_INFO_TYPE_SESSION,
    DP_PROJECT_INFO_TYPE_SNAPSHOT,
    DP_PROJECT_INFO_TYPE_OVERVIEW,
    DP_PROJECT_INFO_TYPE_WORK_TIMES,
} DP_ProjectInfoType;

typedef struct DP_ProjectInfoHeader {
    unsigned int application_id;
    unsigned int user_version;
    const char *path;
} DP_ProjectInfoHeader;

typedef struct DP_ProjectInfoSession {
    long long session_id;
    int source_type;
    const char *source_param;
    const char *protocol;
    unsigned int flags;
    double opened_at;
    double closed_at;
    const unsigned char *thumbnail_data;
    size_t thumbnail_size;
    long long message_count;
    long long continue_count;
    long long snapshot_count;
} DP_ProjectInfoSession;

typedef struct DP_ProjectInfoSnapshot {
    long long snapshot_id;
    long long session_id;
    unsigned int flags;
    double taken_at;
    const unsigned char *thumbnail_data;
    size_t thumbnail_size;
    long long metadata_sequence_id;
    long long continue_session_id;
    long long continue_sequence_id;
    long long snapshot_message_count;
} DP_ProjectInfoSnapshot;

typedef struct DP_ProjectInfoOverview {
    long long session_id;
    const char *protocol;
    double opened_at;
    double closed_at;
    const unsigned char *thumbnail_data;
    size_t thumbnail_size;
} DP_ProjectInfoOverview;

typedef struct DP_ProjectInfoWorkTimes {
    long long session_id;
    long long own_work_minutes;
} DP_ProjectInfoWorkTimes;

typedef struct DP_ProjectInfo {
    DP_ProjectInfoType type;
    union {
        DP_ProjectInfoHeader header;
        DP_ProjectInfoSession session;
        DP_ProjectInfoSnapshot snapshot;
        DP_ProjectInfoOverview overview;
        DP_ProjectInfoWorkTimes work_times;
    };
} DP_ProjectInfo;

// Warning handler when loading a canvas. The warn parameter is one of the
// DP_PROJECT_CANVAS_LOAD_WARN_* constants. Return true if loading should be
// cancelled, false to keep going. If cancelling, you should call DP_error_set
// as well to avoid getting hit with some stale error message down the line.
typedef bool (*DP_ProjectCanvasLoadWarnFn)(void *user, int warn);

// Timelapse callback. Takes ownership of the canvas state, so it must decrement
// its reference count. Must return true to let playback continue and false to
// cancel and bail out of the process.
typedef bool (*DP_ProjectPlaybackCallbackFn)(void *user, int current_frame,
                                             DP_CanvasState *cs,
                                             const DP_Rect *crop_or_null);


// Check whether the given buffer looks like the header of project file. The
// buffer needs to be at least 72 bytes long, anything less will return
// DP_PROJECT_CHECK_HEADER.
DP_ProjectCheckResult DP_project_check(const unsigned char *buf, size_t size);

// Opens the file, reads the first 72 bytes, then calls DP_project_check. The
// OPEN and READ errors can only happen when using this function.
DP_ProjectCheckResult DP_project_check_path(const char *path);

// Tries to check whether the project contains only snapshots or whether there's
// any sessions inside it. This is used to determine whether the file overwrite
// prompt should ask about appending to the project or not.
DP_ProjectAppendStatus DP_project_append_status(const char *path);

DP_ProjectOpenResult DP_project_open(const char *path, unsigned int flags);

bool DP_project_close(DP_Project *prj);


// Set the cancel flag on the project, issue an interrupt to the SQLite
// database. Only operations that mention that they are cancelable in their
// comment handle this gracefully!
void DP_project_cancel(DP_Project *prj);


// [cancelable] Runs SQLite's quick or full integrity check on the project
// database, bailing out at the first detected corruption.
DP_ProjectVerifyStatus DP_project_verify(DP_Project *prj, unsigned int flags);


long long DP_project_session_id(DP_Project *prj);

// Opens a new session. Returns 0 on success and a negative
// DP_PROJECT_SESSION_OPEN_ERROR_* value on failure. There must not be an open
// session yet.
int DP_project_session_open(DP_Project *prj, int source_type,
                            const char *source_param, const char *protocol,
                            unsigned int flags);

// Closes the currently open session, ORing in the given flags to set. Returns 0
// on success, a negative DP_PROJECT_SESSION_CLOSE_ERROR_* value on failure and
// DP_PROJECT_SESSION_CLOSE_NOT_OPEN if there's not session to close.
int DP_project_session_close(DP_Project *prj, unsigned int flags_to_set);

// Records a message to the current session. Returns 0 on success and a negative
// DP_PROJECT_MESSAGE_RECORD_ERROR_* value on failure. A session must be open.
int DP_project_message_record(DP_Project *prj, DP_Message *msg,
                              unsigned int flags);

// Like DP_project_message_record, but for internal messages like resets.
int DP_project_message_internal_record(DP_Project *prj, int type,
                                       unsigned int context_id,
                                       const void *body_or_null, size_t size,
                                       unsigned int flags);


DP_ProjectSessionTimes DP_project_session_times_null(void);

int DP_project_session_times_update(DP_Project *prj,
                                    DP_ProjectSessionTimes *pst);


// Opens a snapshot to record messages to. Returns a positive snapshot id on
// success and a negative DP_PROJECT_SNAPSHOT_OPEN_ERROR_* value on failure.
// A session must be open and no snapshot must be open yet.
long long DP_project_snapshot_open(DP_Project *prj, unsigned int flags);

// Records a message to the given snapshot. The snapshot id must match the
// currently open snapshot. This will cause the snapshot to get the flag
// DP_PROJECT_SNAPSHOT_FLAG_HAS_MESSAGES attached to it upon finishing it.
int DP_project_snapshot_message_record(DP_Project *prj, long long snapshot_id,
                                       DP_Message *msg, unsigned int flags);

// Marks the given snapshot completed. Returns 0 on success and a negative
// DP_PROJECT_SNAPSHOT_FINISH_ERROR_* value on failure. The snapshot id must
// match the currently open snapshot.
int DP_project_snapshot_finish(DP_Project *prj, long long snapshot_id);

// Discards the given snapshot, deleting all messages associated with it. You
// can discard any snapshot, not just the one that's currently open. Returns 0
// on success, a negative DP_PROJECT_SNAPSHOT_DISCARD_ERROR value on failure and
// DP_PROJECT_SNAPSHOT_DISCARD_NOT_FOUND if no such snapshot existed.
int DP_project_snapshot_discard(DP_Project *prj, long long snapshot_id);

// Discards all snapshots and messages associated with them except the one with
// the given id and any snapshots that have the PERSISTENT flag set. Returns the
// number of snapshots discarded on success and a negative
// DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR value on failure.
int DP_project_snapshot_discard_all_except(DP_Project *prj,
                                           long long snapshot_id);

int DP_project_snapshot_canvas(DP_Project *prj, long long snapshot_id,
                               DP_CanvasState *cs,
                               bool (*thumb_write_fn)(void *, DP_Image *,
                                                      DP_Output *),
                               void *thumb_write_user);


// Attaches the given database and writes the messages of the current session
// (unless the DP_PROJECT_SAVE_FLAG_NO_MESSAGES is passed) and a snapshot of the
// given canvas state into it. The target file should first be copied if it
// already exists, this function doesn't take care of rollbacks. If a save has
// been made to the file before and it is the latest session (according to a
// matching source_param on the session), it updates that session by deleting
// any non-persistent snapshots and continuing the messages from the last
// sequence id, otherwise it creates a new one. Returns 0 on success and a
// negative DP_PROJECT_SESSION_SAVE_ERROR_* value on failure.
int DP_project_save(DP_Project *prj, DP_CanvasState *cs, const char *path,
                    unsigned int flags, const char *continue_source_param,
                    long long continue_sequence_id,
                    bool (*thumb_write_fn)(void *, DP_Image *, DP_Output *),
                    void *thumb_write_user);

// Like DP_project_save, but only saves the given canvas state.
int DP_project_save_state(DP_CanvasState *cs, const char *path,
                          bool (*thumb_write_fn)(void *, DP_Image *,
                                                 DP_Output *),
                          void *thumb_write_user);


DP_CanvasState *DP_project_canvas_from_snapshot(DP_Project *prj,
                                                DP_DrawContext *dc,
                                                long long snapshot_id);

DP_CanvasState *DP_project_canvas_from_latest_snapshot(
    DP_Project *prj, DP_DrawContext *dc, bool snapshot_only,
    DP_ProjectCanvasLoadWarnFn warn_fn, void *user,
    char **out_session_source_param, long long *out_session_sequence_id);


// Returns 0 on success and a negative DP_PROJECT_OPEN_ERROR_*,
// DP_PROJECT_SNAPSHOT_CANVAS_ERROR_*, DP_PROJECT_SNAPSHOT_FINISH_ERROR_* or
// DP_PROJECT_CANVAS_SAVE_ERROR_* value on failure. The thumb_write_* parameters
// are optional.
int DP_project_canvas_save(DP_CanvasState *cs, const char *path,
                           bool (*thumb_write_fn)(void *, DP_Image *,
                                                  DP_Output *),
                           void *thumb_write_user);

// Returns 0 on success and a negative DP_PROJECT_OPEN_ERROR_* or
// DP_PROJECT_CANVAS_LOAD_ERROR_* value on failure. On success, the canvas state
// and possibly the session source param and sequence id values will be set.
// It's the callers responsibility to manage the canvas state's reference count
// and DP_free the source param.
DP_ProjectCanvasLoad DP_project_canvas_load(DP_DrawContext *dc,
                                            const char *path,
                                            bool snapshot_only);

// [cancelable] Queries project information according to the given flags, calls
// back the given function accordingly. Returns 0 on success and a negative
// DP_PROJECT_INFO_ERROR_* value on failure.
int DP_project_info(DP_Project *prj, unsigned int flags,
                    void (*callback)(void *, const DP_ProjectInfo *),
                    void *user);

bool DP_project_dump(DP_Project *prj, DP_Output *output);


DP_ProjectPlayback *DP_project_playback_new(DP_Project *prj);

void DP_project_playback_free(DP_ProjectPlayback *pb);

int DP_project_playback_measure(DP_ProjectPlayback *pb, DP_DrawContext *dc,
                                double max_delta_seconds,
                                const DP_Rect *crop_or_null,
                                unsigned int flags);

int DP_project_playback_play(DP_ProjectPlayback *pb, DP_DrawContext *dc,
                             double framerate, double target_seconds,
                             DP_ProjectPlaybackCallbackFn callback, void *user);

const DP_Rect *DP_project_playback_crop_or_null(DP_ProjectPlayback *pb);


#endif
