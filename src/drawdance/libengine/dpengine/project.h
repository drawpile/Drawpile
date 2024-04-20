// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_H
#define DPENGINE_PROJECT_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


#define DP_PROJECT_APPLICATION_ID 520585024
#define DP_PROJECT_USER_VERSION   1

#define DP_PROJECT_CHECK_ERROR_OPEN           -1
#define DP_PROJECT_CHECK_ERROR_READ           -2
#define DP_PROJECT_CHECK_ERROR_HEADER         -3
#define DP_PROJECT_CHECK_ERROR_MAGIC          -4
#define DP_PROJECT_CHECK_ERROR_APPLICATION_ID -5
#define DP_PROJECT_CHECK_ERROR_USER_VERSION   -6

#define DP_PROJECT_OPEN_EXISTING (1 << 0)
#define DP_PROJECT_OPEN_TRUNCATE (1 << 1)

#define DP_PROJECT_VERIFY_QUICK 0
#define DP_PROJECT_VERIFY_FULL  (1 << 0)


typedef struct DP_Project DP_Project;

typedef enum DP_ProjectOpenError {
    DP_PROJECT_OPEN_ERROR_NONE,
    DP_PROJECT_OPEN_ERROR_UNKNOWN,
    DP_PROJECT_OPEN_ERROR_INIT,
    DP_PROJECT_OPEN_ERROR_OPEN,
    DP_PROJECT_OPEN_ERROR_READ_ONLY,
    DP_PROJECT_OPEN_ERROR_SETUP,
    DP_PROJECT_OPEN_ERROR_READ_EMPTY,
    DP_PROJECT_OPEN_ERROR_HEADER_WRITE,
    DP_PROJECT_OPEN_ERROR_HEADER_READ,
    DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH,
    DP_PROJECT_OPEN_ERROR_MIGRATION,
    DP_PROJECT_OPEN_ERROR_PREPARE,
} DP_ProjectOpenError;

typedef struct DP_ProjectOpenResult {
    DP_Project *project;
    DP_ProjectOpenError error;
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


DP_ProjectVerifyStatus DP_project_verify(DP_Project *prj, int flags);


long long DP_project_session_id(DP_Project *prj);

bool DP_project_session_open(DP_Project *prj, DP_ProjectSourceType source_type,
                             const char *source_param, const char *protocol);

// Returns 1 on successful close, 0 on error, -1 if there's nothing to close.
int DP_project_session_close(DP_Project *prj);


bool DP_project_message_record(DP_Project *prj, DP_Message *msg);


bool DP_project_dump(DP_Project *prj, DP_Output *output);


#endif
