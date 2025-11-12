// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_WORKER_H
#define DPENGINE_PROJECT_WORKER_H
#include <dpcommon/common.h>


typedef struct DP_ProjectWorker DP_ProjectWorker;

typedef enum DP_ProjectWorkerEventType {
    DP_PROJECT_WORKER_EVENT_SYNC,
    DP_PROJECT_WORKER_EVENT_OPEN_ERROR,
    DP_PROJECT_WORKER_EVENT_CLOSE_ERROR,
    DP_PROJECT_WORKER_EVENT_WRITE_ERROR,
} DP_ProjectWorkerEventType;

typedef struct DP_ProjectWorkerEventError {
    unsigned int file_id;
    int error;
    const char *message;
} DP_ProjectWorkerEventError;

typedef struct DP_ProjectWorkerEvent {
    DP_ProjectWorkerEventType type;
    union {
        unsigned int sync_id;
        DP_ProjectWorkerEventError error;
    } DP_ANONYMOUS(data);
} DP_ProjectWorkerEvent;

typedef void (*DP_ProjectWorkerHandleEvent)(void *user,
                                            const DP_ProjectWorkerEvent *event);


DP_ProjectWorker *
DP_project_worker_new(DP_ProjectWorkerHandleEvent handle_event_fn, void *user);

void DP_project_worker_free_join(DP_ProjectWorker *pw);

// Triggers a sync event with the given id when the handler thread gets to it.
void DP_project_worker_sync(DP_ProjectWorker *pw, unsigned int sync_id);

// Returns an id for the opened file, which is used to identify subsequent
// outgoing events and incoming commands. Anything that doesn't pertain to the
// current file is ignored.
unsigned int DP_project_worker_open(DP_ProjectWorker *pw, const char *path,
                                    unsigned int flags);

void DP_project_worker_close(DP_ProjectWorker *pw, unsigned int file_id);

void DP_project_worker_session_open(DP_ProjectWorker *pw, unsigned int file_id,
                                    int source_type, const char *source_param,
                                    const char *protocol, unsigned int flags);

void DP_project_worker_session_close(DP_ProjectWorker *pw, unsigned int file_id,
                                     unsigned int flags_to_set);


#endif
