// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_H
#define DPENGINE_PROJECT_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


#define DP_PROJECT_APPLICATION_ID 520585024
#define DP_PROJECT_USER_VERSION   1

#define DP_PROJECT_CANVAS_APPLICATION_ID 520585025
#define DP_PROJECT_CANVAS_USER_VERSION   1

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

#define DP_PROJECT_MESSAGE_RECORD_ERROR_UNKNOWN    (-500)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE     (-501)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_NO_SESSION (-502)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_SERIALIZE  (-503)
#define DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE      (-504)

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
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN  (-1001)
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_READY (-1002)
#define DP_PROJECT_SNAPSHOT_CANVAS_ERROR_WRITE     (-1003)

#define DP_PROJECT_CANVAS_SAVE_ERROR_UNKNOWN       (-1100)
#define DP_PROJECT_CANVAS_SAVE_ERROR_OPEN_SNAPSHOT (-1101)
#define DP_PROJECT_CANVAS_SAVE_ERROR_CLOSE_PROJECT (-1102)

#define DP_PROJECT_CANVAS_LOAD_ERROR_UNKNOWN (-1200)
#define DP_PROJECT_CANVAS_LOAD_ERROR_READ    (-1201)

#define DP_PROJECT_OPEN_EXISTING  (1u << 0u)
#define DP_PROJECT_OPEN_TRUNCATE  (1u << 1u)
#define DP_PROJECT_OPEN_READ_ONLY (1u << 2u)

#define DP_PROJECT_VERIFY_QUICK 0u
#define DP_PROJECT_VERIFY_FULL  (1u << 0u)

#define DP_PROJECT_SOURCE_BLANK   1
#define DP_PROJECT_SOURCE_FILE    2
#define DP_PROJECT_SOURCE_SESSION 3

#define DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED (1u << 0u)

#define DP_PROJECT_MESSAGE_FLAG_OWN (1u << 0u)

#define DP_PROJECT_SNAPSHOT_FLAG_COMPLETE   (1u << 0u)
#define DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT (1u << 1u)
#define DP_PROJECT_SNAPSHOT_FLAG_CANVAS     (1u << 2u)


typedef struct DP_Project DP_Project;

typedef enum DP_ProjectCheckType {
    DP_PROJECT_CHECK_NONE,    // Not a project file.
    DP_PROJECT_CHECK_PROJECT, // Full project file.
    DP_PROJECT_CHECK_CANVAS,  // Single snapshot file.
} DP_ProjectCheckType;

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

typedef enum DP_ProjectVerifyStatus {
    DP_PROJECT_VERIFY_OK,
    DP_PROJECT_VERIFY_CORRUPTED,
    DP_PROJECT_VERIFY_ERROR,
} DP_ProjectVerifyStatus;


// Check whether the given buffer looks like the header of project file. The
// buffer needs to be at least 72 bytes long, anything less will return
// DP_PROJECT_CHECK_HEADER.
DP_ProjectCheckResult DP_project_check(const unsigned char *buf, size_t size);

// Opens the file, reads the first 72 bytes, then calls DP_project_check. The
// OPEN and READ errors can only happen when using this function.
DP_ProjectCheckResult DP_project_check_path(const char *path);

DP_ProjectOpenResult DP_project_open(const char *path, unsigned int flags);

bool DP_project_close(DP_Project *prj);


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


// Opens a snapshot to record messages to. Returns a positive snapshot id on
// success and a negative DP_PROJECT_SNAPSHOT_OPEN_ERROR_* value on failure.
// A session must be open and no snapshot must be open yet.
long long DP_project_snapshot_open(DP_Project *prj, unsigned int flags);

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
// the given id and any snapshots that have the PERSISTENT flag set. At the time
// of writing, we never set this flag, but it's there for forward-compatibility.
// Returns the number of snapshots discarded on success and a negative
// DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR value on failure.
int DP_project_snapshot_discard_all_except(DP_Project *prj,
                                           long long snapshot_id);

int DP_project_snapshot_canvas(DP_Project *prj, long long snapshot_id,
                               DP_CanvasState *cs,
                               bool (*thumb_write)(DP_Image *, DP_Output *));


DP_CanvasState *DP_project_canvas_from_snapshot(DP_Project *prj,
                                                DP_DrawContext *dc,
                                                long long snapshot_id);

DP_CanvasState *DP_project_canvas_from_latest_snapshot(DP_Project *prj,
                                                       DP_DrawContext *dc);


// Returns 0 on success and a negative DP_PROJECT_OPEN_ERROR_*,
// DP_PROJECT_SNAPSHOT_CANVAS_ERROR_*, DP_PROJECT_SNAPSHOT_FINISH_ERROR_* or
// DP_PROJECT_CANVAS_SAVE_ERROR_* value on failure. The thumb_write parameter is
// optional.
int DP_project_canvas_save(DP_CanvasState *cs, const char *path,
                           bool (*thumb_write)(DP_Image *, DP_Output *));

// Returns 0 on success and a negative DP_PROJECT_OPEN_ERROR_* or
// DP_PROJECT_CANVAS_LOAD_ERROR_* value on failure. The out_cs parameter is
// required, it will be set on success and left untouched on failure.
int DP_project_canvas_load(DP_DrawContext *dc, const char *path,
                           DP_CanvasState **out_cs);


bool DP_project_dump(DP_Project *prj, DP_Output *output);


#endif
