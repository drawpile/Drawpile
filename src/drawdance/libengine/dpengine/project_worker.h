// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_WORKER_H
#define DPENGINE_PROJECT_WORKER_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_ProjectWorker DP_ProjectWorker;

typedef enum DP_ProjectWorkerEventType {
    DP_PROJECT_WORKER_EVENT_PROJECT_OPEN,
    DP_PROJECT_WORKER_EVENT_PROJECT_CLOSE,
    DP_PROJECT_WORKER_EVENT_SESSION_OPEN,
    DP_PROJECT_WORKER_EVENT_SESSION_CLOSE,
    DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR,
} DP_ProjectWorkerEventType;

typedef struct DP_ProjectWorkerProjectOpenEvent {
    int error;
    int sql_result;
} DP_ProjectWorkerProjectOpenEvent;

typedef struct DP_ProjectWorkerProjectCloseEvent {
    int error;
} DP_ProjectWorkerProjectCloseEvent;

typedef struct DP_ProjectWorkerSessionOpenEvent {
    int error;
} DP_ProjectWorkerSessionOpenEvent;

typedef struct DP_ProjectWorkerSessionCloseEvent {
    int error;
} DP_ProjectWorkerSessionCloseEvent;

typedef struct DP_ProjectWorkerMessageRecordErrorEvent {
    DP_Message *msg;
    int error;
} DP_ProjectWorkerMessageRecordErrorEvent;

typedef struct DP_ProjectWorkerEvent {
    DP_ProjectWorkerEventType type;
    union {
        DP_ProjectWorkerProjectOpenEvent project_open;
        DP_ProjectWorkerProjectCloseEvent project_close;
        DP_ProjectWorkerSessionOpenEvent session_open;
        DP_ProjectWorkerSessionCloseEvent session_close;
        DP_ProjectWorkerMessageRecordErrorEvent message_record_error;
    } DP_ANONYMOUS(event);
} DP_ProjectWorkerEvent;

typedef void (*DP_ProjectWorkerEventFn)(void *user,
                                        DP_ProjectWorkerEvent *event);


DP_ProjectWorker *DP_project_worker_new(DP_ProjectWorkerEventFn fn, void *user);

void DP_project_worker_free_join(DP_ProjectWorker *pw);


void DP_project_worker_project_open(DP_ProjectWorker *pw, const char *path,
                                    unsigned int flags);

void DP_project_worker_project_close(DP_ProjectWorker *pw);

void DP_project_worker_session_open(DP_ProjectWorker *pw, int source_type,
                                    const char *source_param,
                                    const char *protocol, unsigned int flags);

void DP_project_worker_session_close(DP_ProjectWorker *pw,
                                     unsigned int flags_to_set);

void DP_project_worker_message_record_noinc(DP_ProjectWorker *pw,
                                            DP_Message *msg,
                                            unsigned int flags);

void DP_project_worker_message_record_inc(DP_ProjectWorker *pw, DP_Message *msg,
                                          unsigned int flags);


#endif
