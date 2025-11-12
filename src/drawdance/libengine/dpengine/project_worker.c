// SPDX-License-Identifier: GPL-3.0-or-later
#include "project_worker.h"
#include "project.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>

#define DP_PROJECT_WORKER_DEBUG_ENABLED 1
#ifdef DP_PROJECT_WORKER_DEBUG_ENABLED
#    define DP_PROJECT_WORKER_DEBUG(...)               \
        do {                                           \
            DP_debug("[project worker] " __VA_ARGS__); \
        } while (0)
#else
#    define DP_PROJECT_WORKER_DEBUG(...) \
        do {                             \
            /* nothing */                \
        } while (0)
#endif


struct DP_ProjectWorker {
    DP_ProjectWorkerHandleEvent handle_event_fn;
    void *user;
    DP_Mutex *mutex;
    DP_Semaphore *sem;
    DP_Thread *thread;
    DP_Project *prj;
    long long session_id;
    unsigned int last_file_id;
    unsigned int open_file_id;
    DP_Queue queue;
};

typedef enum DP_ProjectWorkerCommandType {
    DP_PROJECT_WORKER_COMMAND_SYNC,
    DP_PROJECT_WORKER_COMMAND_OPEN,
    DP_PROJECT_WORKER_COMMAND_CLOSE,
    DP_PROJECT_WORKER_COMMAND_SESSION_OPEN,
    DP_PROJECT_WORKER_COMMAND_SESSION_CLOSE,
} DP_ProjectWorkerCommandType;

typedef struct DP_ProjectWorkerCommand {
    DP_ProjectWorkerCommandType type;
    union {
        unsigned int sync_id;
        struct {
            char *path;
            unsigned int flags;
            unsigned int file_id;
        } open;
        struct {
            unsigned int file_id;
        } close;
        struct {
            char *source_param;
            char *protocol;
            int source_type;
            unsigned int flags;
            unsigned int file_id;
        } session_open;
        struct {
            unsigned int flags_to_set;
            unsigned int file_id;
        } session_close;
    };
} DP_ProjectWorkerCommand;


static bool shift_command(DP_Mutex *mutex, DP_Queue *queue,
                          DP_ProjectWorkerCommand *out_command)
{
    DP_MUTEX_MUST_LOCK(mutex);
    DP_ProjectWorkerCommand *command = DP_queue_peek(queue, sizeof(*command));
    bool have_command = command != NULL;
    if (have_command) {
        *out_command = *command;
        DP_queue_shift(queue);
    }
    DP_MUTEX_MUST_UNLOCK(mutex);
    return have_command;
}

static void emit_event(DP_ProjectWorker *pw, DP_ProjectWorkerEvent event)
{
    DP_PROJECT_WORKER_DEBUG("emit event %d", (int)event.type);
    pw->handle_event_fn(pw->user, &event);
}

static void handle_close(DP_ProjectWorker *pw, unsigned int file_id)
{
    if (file_id != pw->open_file_id) {
        DP_warn("Not closing file id %u, currently open is %u", file_id,
                pw->open_file_id);
        return;
    }

    if (!pw->prj) {
        DP_warn("No project open for file id %u", file_id);
        pw->open_file_id = 0u;
        return;
    }

    bool ok = DP_project_close(pw->prj);
    pw->prj = NULL;
    pw->open_file_id = 0u;
    if (!ok) {
        emit_event(
            pw, (DP_ProjectWorkerEvent){DP_PROJECT_WORKER_EVENT_CLOSE_ERROR,
                                        {.error = {file_id, 0, DP_error()}}});
    }
}

static void handle_open(DP_ProjectWorker *pw, unsigned int file_id, char *path,
                        unsigned int flags)
{
    if (pw->prj) {
        DP_warn("Project %u already open, closing to open %u '%s'",
                pw->open_file_id, file_id, path);
        handle_close(pw, file_id);
    }

    DP_ProjectOpenResult result = DP_project_open(path, flags);
    free(path);

    if (result.project) {
        pw->prj = result.project;
        pw->open_file_id = file_id;
    }
    else {
        pw->prj = NULL;
        pw->open_file_id = 0u;
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_OPEN_ERROR,
                           .error = {file_id, result.error, DP_error()}});
    }
}

static void handle_command(DP_ProjectWorker *pw,
                           const DP_ProjectWorkerCommand *command)
{
    switch (command->type) {
    case DP_PROJECT_WORKER_COMMAND_SYNC:
        DP_PROJECT_WORKER_DEBUG("handle sync %u", command->sync_id);
        emit_event(pw, (DP_ProjectWorkerEvent){DP_PROJECT_WORKER_EVENT_SYNC,
                                               {.sync_id = command->sync_id}});
        return;
    case DP_PROJECT_WORKER_COMMAND_OPEN:
        DP_PROJECT_WORKER_DEBUG("handle open %u '%s' flags %x",
                                command->open.file_id, command->open.path,
                                command->open.flags);
        handle_open(pw, command->open.file_id, command->open.path,
                    command->open.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_CLOSE:
        DP_PROJECT_WORKER_DEBUG("handle close %u", command->close.file_id);
        handle_close(pw, command->close.file_id);
        return;
    case DP_PROJECT_WORKER_COMMAND_SESSION_OPEN:
        DP_PROJECT_WORKER_DEBUG("handle session open %u",
                                command->session_open.file_id);
        handle_session_open(pw, command->session_open.file_id,
                            command->session_open.source_type,
                            command->session_open.source_param,
                            command->session_open.protocol,
                            command->session_open.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_SESSION_CLOSE:
        DP_PROJECT_WORKER_DEBUG("handle session close %u",
                                command->session_close.file_id);
        handle_session_close(pw, command->session_close.file_id,
                             command->session_close.flags_to_set);
        return;
    }
    DP_warn("Unhandled project worker command %d", (int)command->type);
}

static void run_worker(void *user)
{
    DP_PROJECT_WORKER_DEBUG("starting");
    DP_ProjectWorker *pw = user;
    DP_Mutex *mutex = pw->mutex;
    DP_Semaphore *sem = pw->sem;
    DP_Queue *queue = &pw->queue;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        DP_ProjectWorkerCommand command;
        if (shift_command(mutex, queue, &command)) {
            handle_command(pw, &command);
        }
        else {
            break;
        }
    }
    DP_PROJECT_WORKER_DEBUG("exiting");
}


static void push_command(DP_ProjectWorker *pw, DP_ProjectWorkerCommand command)
{
    DP_Mutex *mutex = pw->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    *(DP_ProjectWorkerCommand *)DP_queue_push(&pw->queue, sizeof(command)) =
        command;
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_SEMAPHORE_MUST_POST(pw->sem);
}


DP_ProjectWorker *
DP_project_worker_new(DP_ProjectWorkerHandleEvent handle_event_fn, void *user)
{
    DP_ASSERT(handle_event_fn);

    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return NULL;
    }

    DP_Semaphore *sem = DP_semaphore_new(0);
    if (!sem) {
        DP_mutex_free(mutex);
        return NULL;
    }

    DP_ProjectWorker *pw = DP_malloc(sizeof(*pw));
    *pw = (DP_ProjectWorker){
        handle_event_fn, user, mutex, sem, NULL, NULL, 0LL, 0u, 0u,
        DP_QUEUE_NULL};
    DP_queue_init(&pw->queue, 1024, sizeof(DP_ProjectWorkerCommand));

    DP_PROJECT_WORKER_DEBUG("spawn");
    pw->thread = DP_thread_new(run_worker, pw);
    if (!pw->thread) {
        DP_queue_dispose(&pw->queue);
        DP_free(pw);
        DP_semaphore_free(sem);
        DP_mutex_free(mutex);
        return NULL;
    }

    return pw;
}

void DP_project_worker_free_join(DP_ProjectWorker *pw)
{
    if (pw) {
        DP_PROJECT_WORKER_DEBUG("join");
        DP_SEMAPHORE_MUST_POST(pw->sem);
        DP_thread_free_join(pw->thread);
        DP_PROJECT_WORKER_DEBUG("free");
        DP_queue_dispose(&pw->queue);
        DP_semaphore_free(pw->sem);
        DP_mutex_free(pw->mutex);
        DP_free(pw);
    }
}

void DP_project_worker_sync(DP_ProjectWorker *pw, unsigned int sync_id)
{
    DP_ASSERT(pw);
    DP_PROJECT_WORKER_DEBUG("push sync");
    push_command(pw, (DP_ProjectWorkerCommand){DP_PROJECT_WORKER_COMMAND_SYNC,
                                               {.sync_id = sync_id}});
}

unsigned int DP_project_worker_open(DP_ProjectWorker *pw, const char *path,
                                    unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(path);
    unsigned int file_id = ++pw->last_file_id;
    if (file_id == 0) { // In case someone manages to open UINT_MAX files.
        file_id = ++pw->last_file_id;
    }
    DP_PROJECT_WORKER_DEBUG("push open %u %s", file_id, path);
    push_command(pw, (DP_ProjectWorkerCommand){
                         DP_PROJECT_WORKER_COMMAND_OPEN,
                         {.open = {DP_strdup(path), flags, file_id}}});
    return file_id;
}

void DP_project_worker_close(DP_ProjectWorker *pw, unsigned int file_id)
{
    DP_PROJECT_WORKER_DEBUG("push close %u", file_id);
    push_command(pw, (DP_ProjectWorkerCommand){DP_PROJECT_WORKER_COMMAND_CLOSE,
                                               .close = {file_id}});
}

void DP_project_worker_session_open(DP_ProjectWorker *pw, unsigned int file_id,
                                    int source_type, const char *source_param,
                                    const char *protocol, unsigned int flags)
{
    DP_ASSERT(pw);
    push_command(
        pw, (DP_ProjectWorkerCommand){
                DP_PROJECT_WORKER_COMMAND_SESSION_OPEN,
                .session_open = {DP_strdup(source_param), DP_strdup(protocol),
                                 source_type, flags, file_id}});
}

void DP_project_worker_session_close(DP_ProjectWorker *pw, unsigned int file_id,
                                     unsigned int flags_to_set)
{
    DP_ASSERT(pw);
    push_command(pw, (DP_ProjectWorkerCommand){
                         DP_PROJECT_WORKER_COMMAND_SESSION_CLOSE,
                         .session_close = {flags_to_set, file_id}});
}
