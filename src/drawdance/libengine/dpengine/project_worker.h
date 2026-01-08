// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_WORKER_H
#define DPENGINE_PROJECT_WORKER_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Image DP_Image;
typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


typedef struct DP_ProjectWorker DP_ProjectWorker;

typedef enum DP_ProjectWorkerEventType {
    DP_PROJECT_WORKER_EVENT_OPEN_ERROR,
    DP_PROJECT_WORKER_EVENT_CLOSE_ERROR,
    DP_PROJECT_WORKER_EVENT_WRITE_ERROR,
    DP_PROJECT_WORKER_EVENT_SESSION_OPEN_ERROR,
    DP_PROJECT_WORKER_EVENT_SESSION_CLOSE_ERROR,
    DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR,
    DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR,
    DP_PROJECT_WORKER_EVENT_THUMBNAIL_MAKE_ERROR,
    DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE_ERROR,
    DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE,
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
        long long session_id;
        long long own_work_minutes;
        DP_ProjectWorkerEventError error;
    } DP_ANONYMOUS(data);
} DP_ProjectWorkerEvent;

typedef void (*DP_ProjectWorkerHandleEventFn)(
    void *user, const DP_ProjectWorkerEvent *event);

typedef bool (*DP_ProjectWorkerThumbWriteFn)(void *user, DP_Image *img);

typedef void (*DP_ProjectWorkerSyncFn)(void *user);


DP_ProjectWorker *
DP_project_worker_new(DP_ProjectWorkerHandleEventFn handle_event_fn,
                      DP_ProjectWorkerThumbWriteFn thumb_write_fn, void *user);

void DP_project_worker_free_join(DP_ProjectWorker *pw);

// Calls the given sync function when the worker gets to it.
void DP_project_worker_sync(DP_ProjectWorker *pw, DP_ProjectWorkerSyncFn fn,
                            void *user);

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

void DP_project_worker_message_record_noinc(DP_ProjectWorker *pw,
                                            unsigned int file_id,
                                            DP_Message *msg,
                                            unsigned int flags);

void DP_project_worker_message_record_inc(DP_ProjectWorker *pw,
                                          unsigned int file_id, DP_Message *msg,
                                          unsigned int flags);

// Takes ownership of body and DP_frees it later.
void DP_project_worker_message_internal_record(DP_ProjectWorker *pw,
                                               unsigned int file_id, int type,
                                               unsigned int context_id,
                                               void *body_or_null, size_t size,
                                               unsigned int flags);

void DP_project_worker_snapshot_open_noinc(DP_ProjectWorker *pw,
                                           unsigned int file_id,
                                           DP_CanvasState *cs,
                                           unsigned int flags);

void DP_project_worker_snapshot_open_inc(DP_ProjectWorker *pw,
                                         unsigned int file_id,
                                         DP_CanvasState *cs,
                                         unsigned int flags);

void DP_project_worker_snapshot_message_record_noinc(DP_ProjectWorker *pw,
                                                     unsigned int file_id,
                                                     DP_Message *msg,
                                                     unsigned int flags);

void DP_project_worker_snapshot_finish(DP_ProjectWorker *pw,
                                       unsigned int file_id,
                                       bool discard_other_snapshots);

void DP_project_worker_thumbnail_make_noinc(DP_ProjectWorker *pw,
                                            unsigned int file_id,
                                            DP_CanvasState *cs);

void DP_project_worker_session_times_update(DP_ProjectWorker *pw,
                                            unsigned int file_id);

#endif
