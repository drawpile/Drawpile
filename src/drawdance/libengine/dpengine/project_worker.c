// SPDX-License-Identifier: GPL-3.0-or-later
#include "project_worker.h"
#include "canvas_state.h"
#include "image.h"
#include "project.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>

#ifdef DP_PROJECT_WORKER_DEBUG_ENABLED
#    define DP_PROJECT_WORKER_DEBUG(...)               \
        do {                                           \
            DP_debug("[project worker] " __VA_ARGS__); \
        } while (0)

static const char *debug_str(const char *s)
{
    if (s) {
        return s;
    }
    else {
        return "";
    }
}
#else
#    define DP_PROJECT_WORKER_DEBUG(...) \
        do {                             \
            /* nothing */                \
        } while (0)
#endif


struct DP_ProjectWorker {
    DP_ProjectWorkerHandleEventFn handle_event_fn;
    DP_ProjectWorkerThumbWriteFn thumb_write_fn;
    void *user;
    DP_Mutex *mutex;
    DP_Semaphore *sem;
    DP_Thread *thread;
    DP_Project *prj;
    DP_ProjectSessionTimes pst;
    unsigned int last_file_id;
    unsigned int open_file_id;
    long long open_snapshot_id;
    DP_Queue queue;
};

typedef enum DP_ProjectWorkerCommandType {
    DP_PROJECT_WORKER_COMMAND_SYNC,
    DP_PROJECT_WORKER_COMMAND_OPEN,
    DP_PROJECT_WORKER_COMMAND_CLOSE,
    DP_PROJECT_WORKER_COMMAND_SESSION_OPEN,
    DP_PROJECT_WORKER_COMMAND_SESSION_CLOSE,
    DP_PROJECT_WORKER_COMMAND_MESSAGE_RECORD,
    DP_PROJECT_WORKER_COMMAND_MESSAGE_INTERNAL_RECORD,
    DP_PROJECT_WORKER_COMMAND_SNAPSHOT_OPEN,
    DP_PROJECT_WORKER_COMMAND_SNAPSHOT_MESSAGE_RECORD,
    DP_PROJECT_WORKER_COMMAND_SNAPSHOT_FINISH,
    DP_PROJECT_WORKER_COMMAND_THUMBNAIL_MAKE,
    DP_PROJECT_WORKER_COMMAND_SESSION_TIMES_UPDATE,
} DP_ProjectWorkerCommandType;

typedef struct DP_ProjectWorkerCommand {
    DP_ProjectWorkerCommandType type;
    union {
        struct {
            DP_ProjectWorkerSyncFn fn;
            void *user;
        } sync;
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
        struct {
            DP_Message *msg;
            unsigned int flags;
            unsigned int file_id;
        } message_record;
        struct {
            void *body_or_null;
            size_t size;
            int type;
            unsigned int context_id;
            unsigned int flags;
            unsigned int file_id;
        } message_internal_record;
        struct {
            DP_CanvasState *cs;
            unsigned int flags;
            unsigned int file_id;
        } snapshot_open;
        struct {
            DP_Message *msg;
            unsigned int flags;
            unsigned int file_id;
        } snapshot_message_record;
        struct {
            unsigned int file_id;
            bool discard_other_snapshots;
        } snapshot_finish;
        struct {
            DP_CanvasState *cs;
            unsigned int file_id;
        } thumbnail_make;
        struct {
            unsigned int file_id;
        } session_times_update;
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
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_warn("Not closing file id %u, currently open is %u", file_id,
                open_file_id);
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
        unsigned int open_file_id = pw->open_file_id;
        DP_warn("Project %u already open, closing to open %u '%s'",
                open_file_id, file_id, path);
        handle_close(pw, open_file_id);
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

static void handle_session_open(DP_ProjectWorker *pw, unsigned int file_id,
                                int source_type, char *source_param,
                                char *protocol, unsigned int flags)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_free(source_param);
        DP_free(protocol);
        DP_warn("Not opening session on file id %u, currently open is %u",
                file_id, open_file_id);
        return;
    }

    DP_Project *prj = pw->prj;
    int result = DP_project_session_open(prj, source_type, source_param,
                                         protocol, flags);
    DP_free(source_param);
    DP_free(protocol);
    if (result == 0) {
        DP_PROJECT_WORKER_DEBUG("opened session %lld",
                                DP_project_session_id(prj));
        pw->pst = DP_project_session_times_null();
    }
    else {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SESSION_OPEN_ERROR,
                           .error = {file_id, result, DP_error()}});
    }
}

static void handle_session_close(DP_ProjectWorker *pw, unsigned int file_id,
                                 unsigned int flags_to_set)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_warn("Not closing session on file id %u, currently open is %u",
                file_id, open_file_id);
        return;
    }

    DP_Project *prj = pw->prj;
    DP_PROJECT_WORKER_DEBUG("closing session %lld", DP_project_session_id(prj));
    int result = DP_project_session_close(prj, flags_to_set);
    if (result != 0) {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SESSION_CLOSE_ERROR,
                           .error = {file_id, result, DP_error()}});
    }
}

static void handle_message_record(DP_ProjectWorker *pw, unsigned int file_id,
                                  DP_Message *msg, unsigned int flags)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_message_decref(msg);
        DP_warn("Not recording message on file id %u, currently open is %u",
                file_id, open_file_id);
        return;
    }

    int result = DP_project_message_record(pw->prj, msg, flags);
    DP_message_decref(msg);
    if (result != 0) {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR,
                           .error = {file_id, result, DP_error()}});
    }
}


static void handle_message_internal_record(DP_ProjectWorker *pw,
                                           unsigned int file_id, int type,
                                           unsigned int context_id,
                                           void *body_or_null, size_t size,
                                           unsigned int flags)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_free(body_or_null);
        DP_warn("Not recording internal message on file id %u, currently open "
                "is %u",
                file_id, open_file_id);
        return;
    }

    int result = DP_project_message_internal_record(pw->prj, type, context_id,
                                                    body_or_null, size, flags);
    DP_free(body_or_null);
    if (result != 0) {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR,
                           .error = {file_id, result, DP_error()}});
    }
}

static void try_discard_snapshot(DP_Project *prj, long long snapshot_id)
{
    DP_PROJECT_WORKER_DEBUG("discarding failed snapshot %lld", snapshot_id);
    int result = DP_project_snapshot_discard(prj, snapshot_id);
    if (result != 0) {
        DP_warn("Error %d discarding failed snapshot: %s", result, DP_error());
    }
}

static void handle_snapshot_open(DP_ProjectWorker *pw, unsigned int file_id,
                                 DP_CanvasState *cs, unsigned int flags)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_canvas_state_decref(cs);
        DP_warn("Not creating snapshot on file id %u, currently open is %u",
                file_id, open_file_id);
        return;
    }

    DP_Project *prj = pw->prj;
    long long prev_snapshot_id = pw->open_snapshot_id;
    if (prev_snapshot_id != 0LL) {
        DP_warn("Snapshot %lld already open on file id %u, discarding it",
                prev_snapshot_id, open_file_id);
        try_discard_snapshot(prj, prev_snapshot_id);
        pw->open_snapshot_id = 0LL;
    }

    long long snapshot_id = DP_project_snapshot_open(prj, flags);
    if (snapshot_id < 0LL) {
        DP_canvas_state_decref(cs);
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR,
                           .error = {file_id, DP_llong_to_int(snapshot_id),
                                     DP_error()}});
        return;
    }

    DP_PROJECT_WORKER_DEBUG("creating snapshot %lld", snapshot_id);
    int result = DP_project_snapshot_canvas(prj, snapshot_id, cs, NULL, NULL);
    DP_canvas_state_decref(cs);

    if (result == 0) {
        pw->open_snapshot_id = snapshot_id;
    }
    else {
        try_discard_snapshot(prj, snapshot_id);
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR,
                           .error = {file_id, result, DP_error()}});
    }
}

static void handle_snapshot_message_record(DP_ProjectWorker *pw,
                                           unsigned int file_id,
                                           DP_Message *msg, unsigned int flags)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_message_decref(msg);
        DP_warn("Not recording snapshot message on file id %u, currently open "
                "is %u",
                file_id, open_file_id);
        return;
    }

    long long snapshot_id = pw->open_snapshot_id;
    if (snapshot_id == 0LL) {
        DP_message_decref(msg);
        DP_PROJECT_WORKER_DEBUG(
            "not recording snapshot message file id %u because none is open",
            file_id);
        return;
    }

    int result =
        DP_project_snapshot_message_record(pw->prj, snapshot_id, msg, flags);
    DP_message_decref(msg);
    if (result != 0) {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR,
                           .error = {file_id, result, DP_error()}});
    }
}

static void handle_snapshot_finish(DP_ProjectWorker *pw, unsigned int file_id,
                                   bool discard_other_snapshots)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_warn("Not finishing snapshot on file id %u, currently open is %u",
                file_id, open_file_id);
        return;
    }

    long long snapshot_id = pw->open_snapshot_id;
    if (snapshot_id == 0LL) {
        DP_PROJECT_WORKER_DEBUG(
            "not finishing snapshot on file id %u because none is open",
            file_id);
        return;
    }

    DP_PROJECT_WORKER_DEBUG("finishing snapshot %lld", snapshot_id);
    DP_Project *prj = pw->prj;
    int finish_result = DP_project_snapshot_finish(pw->prj, snapshot_id);
    pw->open_snapshot_id = 0LL;

    if (finish_result != 0) {
        try_discard_snapshot(prj, snapshot_id);
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SNAPSHOT_ERROR,
                           .error = {file_id, finish_result, DP_error()}});
        return;
    }

    if (discard_other_snapshots) {
        DP_PROJECT_WORKER_DEBUG("discarding all snapshots except %lld",
                                snapshot_id);
        int discard_result =
            DP_project_snapshot_discard_all_except(prj, snapshot_id);
        if (discard_result < 0) {
            DP_warn("Failed to discard all snapshots except %lld: %s",
                    snapshot_id, DP_error());
        }
        else {
            DP_PROJECT_WORKER_DEBUG("discarded %d snapshot(s)", discard_result);
        }
    }
}

static void handle_thumbnail_make(DP_ProjectWorker *pw, unsigned int file_id,
                                  DP_CanvasState *cs)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_canvas_state_decref(cs);
        DP_warn("Not creating snapshot on file id %u, currently open is %u",
                file_id, open_file_id);
        return;
    }

    DP_Image *thumb = DP_image_thumbnail_from_canvas(
        cs, NULL, DP_PROJECT_THUMBNAIL_SIZE, DP_PROJECT_THUMBNAIL_SIZE);
    DP_canvas_state_decref(cs);

    bool ok = thumb && pw->thumb_write_fn(pw->user, thumb);
    DP_image_free(thumb);
    if (!ok) {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_THUMBNAIL_MAKE_ERROR,
                           .error = {file_id, -1, DP_error()}});
    }
}

static void handle_session_times_update(DP_ProjectWorker *pw,
                                        unsigned int file_id)
{
    unsigned int open_file_id = pw->open_file_id;
    if (file_id != open_file_id) {
        DP_warn(
            "Not updating session times on file id %u, currently open is %u",
            file_id, open_file_id);
        return;
    }

    int result = DP_project_session_times_update(pw->prj, &pw->pst);
    if (result == 0) {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE,
                           .own_work_minutes = pw->pst.own_work_minutes});
    }
    else {
        emit_event(pw, (DP_ProjectWorkerEvent){
                           DP_PROJECT_WORKER_EVENT_SESSION_TIMES_UPDATE_ERROR,
                           .error = {file_id, -1, DP_error()}});
    }
}

static void handle_command(DP_ProjectWorker *pw,
                           const DP_ProjectWorkerCommand *command)
{
    switch (command->type) {
    case DP_PROJECT_WORKER_COMMAND_SYNC:
        DP_PROJECT_WORKER_DEBUG("handle sync %p", command->sync.user);
        command->sync.fn(command->sync.user);
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
        DP_PROJECT_WORKER_DEBUG("handle session open %u source_type %d "
                                "source_param '%s' protocol '%s' flags %x",
                                command->session_open.file_id,
                                command->session_open.source_type,
                                debug_str(command->session_open.source_param),
                                debug_str(command->session_open.protocol),
                                command->session_open.flags);
        handle_session_open(pw, command->session_open.file_id,
                            command->session_open.source_type,
                            command->session_open.source_param,
                            command->session_open.protocol,
                            command->session_open.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_SESSION_CLOSE:
        DP_PROJECT_WORKER_DEBUG("handle session close %u flags_to_set %x",
                                command->session_close.file_id,
                                command->session_close.flags_to_set);
        handle_session_close(pw, command->session_close.file_id,
                             command->session_close.flags_to_set);
        return;
    case DP_PROJECT_WORKER_COMMAND_MESSAGE_RECORD:
        DP_PROJECT_WORKER_DEBUG(
            "handle message record %u type %s context_id %u flags 0x%x",
            command->message_record.file_id,
            DP_message_type_enum_name_unprefixed(
                DP_message_type(command->message_record.msg)),
            DP_message_context_id(command->message_record.msg),
            command->message_record.flags);
        handle_message_record(pw, command->message_record.file_id,
                              command->message_record.msg,
                              command->message_record.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_MESSAGE_INTERNAL_RECORD:
        DP_PROJECT_WORKER_DEBUG("handle message internal record %u type %d "
                                "context_id %u flags 0x%x",
                                command->message_internal_record.file_id,
                                command->message_internal_record.type,
                                command->message_internal_record.context_id,
                                command->message_internal_record.flags);
        handle_message_internal_record(
            pw, command->message_internal_record.file_id,
            command->message_internal_record.type,
            command->message_internal_record.context_id,
            command->message_internal_record.body_or_null,
            command->message_internal_record.size,
            command->message_internal_record.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_SNAPSHOT_OPEN:
        DP_PROJECT_WORKER_DEBUG("handle snapshot open %u flags 0x%x",
                                command->snapshot_open.file_id,
                                command->snapshot_open.flags);
        handle_snapshot_open(pw, command->snapshot_open.file_id,
                             command->snapshot_open.cs,
                             command->snapshot_open.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_SNAPSHOT_MESSAGE_RECORD:
        DP_PROJECT_WORKER_DEBUG(
            "handle snapshot message record %u type %s context_id %u flags "
            "0x%x",
            command->snapshot_message_record.file_id,
            DP_message_type_enum_name_unprefixed(
                DP_message_type(command->snapshot_message_record.msg)),
            DP_message_context_id(command->snapshot_message_record.msg),
            command->snapshot_message_record.flags);
        handle_snapshot_message_record(pw,
                                       command->snapshot_message_record.file_id,
                                       command->snapshot_message_record.msg,
                                       command->snapshot_message_record.flags);
        return;
    case DP_PROJECT_WORKER_COMMAND_SNAPSHOT_FINISH:
        DP_PROJECT_WORKER_DEBUG("handle snapshot finish %u",
                                command->snapshot_finish.file_id);
        handle_snapshot_finish(
            pw, command->snapshot_finish.file_id,
            command->snapshot_finish.discard_other_snapshots);
        return;
    case DP_PROJECT_WORKER_COMMAND_THUMBNAIL_MAKE:
        DP_PROJECT_WORKER_DEBUG("handle thumbnail make %u",
                                command->thumbnail_make.file_id);
        handle_thumbnail_make(pw, command->thumbnail_make.file_id,
                              command->thumbnail_make.cs);
        return;
    case DP_PROJECT_WORKER_COMMAND_SESSION_TIMES_UPDATE:
        DP_PROJECT_WORKER_DEBUG("handle session times update %u",
                                command->session_times_update.file_id);
        handle_session_times_update(pw, command->session_times_update.file_id);
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
DP_project_worker_new(DP_ProjectWorkerHandleEventFn handle_event_fn,
                      DP_ProjectWorkerThumbWriteFn thumb_write_fn, void *user)
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
        handle_event_fn,
        thumb_write_fn,
        user,
        mutex,
        sem,
        NULL,
        NULL,
        DP_project_session_times_null(),
        0u,
        0u,
        0LL,
        DP_QUEUE_NULL,
    };
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

        DP_Project *prj = pw->prj;
        if (prj && !DP_project_close(prj)) {
            DP_warn("Failed to close project on free: %s", DP_error());
        }

        DP_free(pw);
    }
}

void DP_project_worker_sync(DP_ProjectWorker *pw, DP_ProjectWorkerSyncFn fn,
                            void *user)
{
    DP_ASSERT(pw);
    DP_PROJECT_WORKER_DEBUG("push sync");
    push_command(pw, (DP_ProjectWorkerCommand){DP_PROJECT_WORKER_COMMAND_SYNC,
                                               {.sync = {fn, user}}});
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

void DP_project_worker_message_record_noinc(DP_ProjectWorker *pw,
                                            unsigned int file_id,
                                            DP_Message *msg, unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(msg);
    push_command(
        pw, (DP_ProjectWorkerCommand){DP_PROJECT_WORKER_COMMAND_MESSAGE_RECORD,
                                      .message_record = {msg, flags, file_id}});
}

void DP_project_worker_message_record_inc(DP_ProjectWorker *pw,
                                          unsigned int file_id, DP_Message *msg,
                                          unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(msg);
    DP_project_worker_message_record_noinc(pw, file_id, DP_message_incref(msg),
                                           flags);
}

void DP_project_worker_message_internal_record(DP_ProjectWorker *pw,
                                               unsigned int file_id, int type,
                                               unsigned int context_id,
                                               void *body_or_null, size_t size,
                                               unsigned int flags)
{
    DP_ASSERT(pw);
    push_command(pw,
                 (DP_ProjectWorkerCommand){
                     DP_PROJECT_WORKER_COMMAND_MESSAGE_INTERNAL_RECORD,
                     .message_internal_record = {body_or_null, size, type,
                                                 context_id, flags, file_id}});
}

void DP_project_worker_snapshot_open_noinc(DP_ProjectWorker *pw,
                                           unsigned int file_id,
                                           DP_CanvasState *cs,
                                           unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(cs);
    push_command(
        pw, (DP_ProjectWorkerCommand){DP_PROJECT_WORKER_COMMAND_SNAPSHOT_OPEN,
                                      .snapshot_open = {cs, flags, file_id}});
}

void DP_project_worker_snapshot_open_inc(DP_ProjectWorker *pw,
                                         unsigned int file_id,
                                         DP_CanvasState *cs, unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(cs);
    DP_project_worker_snapshot_open_noinc(pw, file_id,
                                          DP_canvas_state_incref(cs), flags);
}

void DP_project_worker_snapshot_message_record_noinc(DP_ProjectWorker *pw,
                                                     unsigned int file_id,
                                                     DP_Message *msg,
                                                     unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(msg);
    push_command(pw, (DP_ProjectWorkerCommand){
                         DP_PROJECT_WORKER_COMMAND_SNAPSHOT_MESSAGE_RECORD,
                         .snapshot_message_record = {msg, flags, file_id}});
}

void DP_project_worker_snapshot_finish(DP_ProjectWorker *pw,
                                       unsigned int file_id,
                                       bool discard_other_snapshots)
{
    DP_ASSERT(pw);
    push_command(pw,
                 (DP_ProjectWorkerCommand){
                     DP_PROJECT_WORKER_COMMAND_SNAPSHOT_FINISH,
                     .snapshot_finish = {file_id, discard_other_snapshots}});
}

void DP_project_worker_thumbnail_make_noinc(DP_ProjectWorker *pw,
                                            unsigned int file_id,
                                            DP_CanvasState *cs)
{
    DP_ASSERT(pw);
    DP_ASSERT(cs);
    if (pw->thumb_write_fn) {
        push_command(pw, (DP_ProjectWorkerCommand){
                             DP_PROJECT_WORKER_COMMAND_THUMBNAIL_MAKE,
                             .thumbnail_make = {cs, file_id}});
    }
    else {
        DP_PROJECT_WORKER_DEBUG("no thumb_write_fn for thumbnail_make %u",
                                file_id);
    }
}

void DP_project_worker_session_times_update(DP_ProjectWorker *pw,
                                            unsigned int file_id)
{
    DP_ASSERT(pw);
    push_command(pw, (DP_ProjectWorkerCommand){
                         DP_PROJECT_WORKER_COMMAND_SESSION_TIMES_UPDATE,
                         .session_times_update = {file_id}});
}
