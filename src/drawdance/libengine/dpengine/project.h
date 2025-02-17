// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_H
#define DPENGINE_PROJECT_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


#define DP_PROJECT_APPLICATION_ID 520585024
#define DP_PROJECT_USER_VERSION   1

#define DP_PROJECT_OPEN_ERROR_UNKNOWN         -1
#define DP_PROJECT_OPEN_ERROR_INIT            -2
#define DP_PROJECT_OPEN_ERROR_OPEN            -3
#define DP_PROJECT_OPEN_ERROR_READ_ONLY       -4
#define DP_PROJECT_OPEN_ERROR_SETUP           -5
#define DP_PROJECT_OPEN_ERROR_READ_EMPTY      -6
#define DP_PROJECT_OPEN_ERROR_HEADER_WRITE    -7
#define DP_PROJECT_OPEN_ERROR_HEADER_READ     -8
#define DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH -9
#define DP_PROJECT_OPEN_ERROR_MIGRATION       -10
#define DP_PROJECT_OPEN_ERROR_PREPARE         -11

#define DP_PROJECT_CHECK_ERROR_OPEN           -1
#define DP_PROJECT_CHECK_ERROR_READ           -2
#define DP_PROJECT_CHECK_ERROR_HEADER         -3
#define DP_PROJECT_CHECK_ERROR_MAGIC          -4
#define DP_PROJECT_CHECK_ERROR_APPLICATION_ID -5
#define DP_PROJECT_CHECK_ERROR_USER_VERSION   -6

#define DP_PROJECT_SESSION_OPEN_ERROR_ALREADY_OPEN -1
#define DP_PROJECT_SESSION_OPEN_ERROR_WRITE        -2

#define DP_PROJECT_SESSION_CLOSE_ERROR_WRITE     -1
#define DP_PROJECT_SESSION_CLOSE_ERROR_NO_CHANGE -2
#define DP_PROJECT_SESSION_CLOSE_NOT_OPEN        1

#define DP_PROJECT_MESSAGE_RECORD_ERROR_NO_SESSION -1
#define DP_PROJECT_MESSAGE_RECORD_ERROR_SERIALIZE  -2
#define DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE      -3

#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_NO_SESSION   -1
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_ALREADY_OPEN -2
#define DP_PROJECT_SNAPSHOT_OPEN_ERROR_WRITE        -3

#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN  -1
#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_WRITE     -2
#define DP_PROJECT_SNAPSHOT_FINISH_ERROR_NO_CHANGE -3

#define DP_PROJECT_SNAPSHOT_DISCARD_ERROR_WRITE -1
#define DP_PROJECT_SNAPSHOT_DISCARD_NOT_FOUND   1

#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ       -1
#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_WRITE      -2
#define DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ_WRITE -3

#define DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_NOT_OPEN  -1
#define DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_SERIALIZE -2
#define DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_WRITE     -3

#define DP_PROJECT_OPEN_EXISTING (1u << 0u)
#define DP_PROJECT_OPEN_TRUNCATE (1u << 1u)

#define DP_PROJECT_VERIFY_QUICK 0u
#define DP_PROJECT_VERIFY_FULL  (1u << 0u)

#define DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED (1u << 0u)

#define DP_PROJECT_MESSAGE_FLAG_OWN (1u << 0u)

#define DP_PROJECT_SNAPSHOT_FLAG_COMPLETE   (1u << 0u)
#define DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT (1u << 1u)


typedef struct DP_Project DP_Project;

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

typedef enum DP_ProjectSourceType {
    DP_PROJECT_SOURCE_BLANK,
    DP_PROJECT_SOURCE_FILE_OPEN,
    DP_PROJECT_SOURCE_SESSION_JOIN,
} DP_ProjectSourceType;


// Checks the header of the given file. If it's a valid Drawpile project file,
// it will return the version number, which is a positive integer. Otherwise, it
// will return a negative error number out of DP_PROJECT_CHECK_ERROR_*.
int DP_project_check(const char *path);

DP_ProjectOpenResult DP_project_open(const char *path, unsigned int flags);

bool DP_project_open_was_busy(const DP_ProjectOpenResult *open);

bool DP_project_close(DP_Project *prj);


DP_ProjectVerifyStatus DP_project_verify(DP_Project *prj, unsigned int flags);


long long DP_project_session_id(DP_Project *prj);

// Opens a new session. Returns 0 on success and a negative
// DP_PROJECT_SESSION_OPEN_ERROR_* value on failure. There must not be an open
// session yet.
int DP_project_session_open(DP_Project *prj, DP_ProjectSourceType source_type,
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

// Records a message to the given snapshot. Returns 0 on success and a negative
// DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_* value on failure. The snapshot id
// must match the currently open snapshot.
int DP_project_snapshot_message_record(DP_Project *prj, long long snapshot_id,
                                       DP_Message *msg);


bool DP_project_dump(DP_Project *prj, DP_Output *output);


#endif
