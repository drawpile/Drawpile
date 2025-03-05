// SPDX-License-Identifier: GPL-3.0-or-later
#include "project_worker.h"
#include "project.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>


struct DP_ProjectWorker {
    DP_Mutex *mutex;
    DP_Semaphore *sem;
    DP_Thread *thread;
    DP_Project *prj;
    DP_Queue queue;
    DP_ProjectWorkerEventFn fn;
    void *user;
};

typedef enum DP_ProjectWorkerMessageType {
    DP_PROJECT_WORKER_MESSAGE_STOP,
    DP_PROJECT_WORKER_MESSAGE_PROJECT_OPEN,
    DP_PROJECT_WORKER_MESSAGE_PROJECT_CLOSE,
    DP_PROJECT_WORKER_MESSAGE_SESSION_OPEN,
    DP_PROJECT_WORKER_MESSAGE_SESSION_CLOSE,
    DP_PROJECT_WORKER_MESSAGE_MESSAGE_RECORD,
} DP_ProjectWorkerMessageType;

typedef struct DP_ProjectWorkerMessageProjectOpen {
    char *path;
    unsigned int flags;
} DP_ProjectWorkerMessageProjectOpen;

typedef struct DP_ProjectWorkerMessageSessionOpen {
    int source_type;
    char *source_param;
    char *protocol;
    unsigned int flags;
} DP_ProjectWorkerMessageSessionOpen;

typedef struct DP_ProjectWorkerMessageSessionClose {
    unsigned int flags_to_set;
} DP_ProjectWorkerMessageSessionClose;

typedef struct DP_ProjectWorkerMessageMessageRecord {
    DP_Message *msg;
    unsigned int flags;
} DP_ProjectWorkerMessageMessageRecord;

typedef struct DP_ProjectWorkerMessage {
    DP_ProjectWorkerMessageType type;
    union {
        int dummy; // Make this initializable without the compiler whining.
        DP_ProjectWorkerMessageProjectOpen project_open;
        DP_ProjectWorkerMessageSessionOpen session_open;
        DP_ProjectWorkerMessageSessionClose session_close;
        DP_ProjectWorkerMessageMessageRecord message_record;
    };
} DP_ProjectWorkerMessage;


static void worker_call(DP_ProjectWorker *pw, DP_ProjectWorkerEvent event)
{
    pw->fn(pw->user, &event);
}

static void worker_project_open(DP_ProjectWorker *pw,
                                DP_ProjectWorkerMessageProjectOpen *pwmpo)
{
    if (pw->prj) {
        DP_warn("Project worker closing project to open a new one");
        DP_project_close(pw->prj);
        pw->prj = NULL;
    }

    DP_ProjectOpenResult result = DP_project_open(pwmpo->path, pwmpo->flags);
    pw->prj = result.project;
    DP_free(pwmpo->path);
    worker_call(pw, (DP_ProjectWorkerEvent){
                        DP_PROJECT_WORKER_EVENT_PROJECT_OPEN,
                        .project_open = {result.error, result.sql_result}});
}

static void worker_project_close(DP_ProjectWorker *pw)
{
    int error = DP_project_close(pw->prj);
    pw->prj = NULL;
    worker_call(pw,
                (DP_ProjectWorkerEvent){DP_PROJECT_WORKER_EVENT_PROJECT_CLOSE,
                                        .project_close = {error}});
}

static void worker_session_open(DP_ProjectWorker *pw,
                                DP_ProjectWorkerMessageSessionOpen *pwmso)
{
    int error = DP_project_session_open(pw->prj, pwmso->source_type,
                                        pwmso->source_param, pwmso->protocol,
                                        pwmso->flags);
    DP_free(pwmso->protocol);
    DP_free(pwmso->source_param);
    worker_call(pw,
                (DP_ProjectWorkerEvent){DP_PROJECT_WORKER_EVENT_SESSION_OPEN,
                                        .session_open = {error}});
}

static void worker_session_close(DP_ProjectWorker *pw,
                                 DP_ProjectWorkerMessageSessionClose *pwmsc)
{
    int error = DP_project_session_close(pw->prj, pwmsc->flags_to_set);
    worker_call(pw,
                (DP_ProjectWorkerEvent){DP_PROJECT_WORKER_EVENT_SESSION_CLOSE,
                                        .session_close = {error}});
}

static void worker_message_record(DP_ProjectWorker *pw,
                                  DP_ProjectWorkerMessageMessageRecord *pwmmr)
{
    DP_Message *msg = pwmmr->msg;
    int error = DP_project_message_record(pw->prj, pwmmr->msg, pwmmr->flags);
    if (error != 0) {
        worker_call(pw, (DP_ProjectWorkerEvent){
                            DP_PROJECT_WORKER_EVENT_MESSAGE_RECORD_ERROR,
                            .message_record_error = {msg, error}});
    }
}

static void worker_run(void *user)
{
    DP_ProjectWorker *pw = user;
    DP_Mutex *mutex = pw->mutex;
    DP_Semaphore *sem = pw->sem;
    DP_Queue *queue = &pw->queue;
    bool running = true;
    while (running) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        DP_MUTEX_MUST_LOCK(mutex);
        DP_ProjectWorkerMessage pwm =
            *(DP_ProjectWorkerMessage *)DP_queue_peek(queue, sizeof(pwm));
        DP_queue_shift(queue);
        DP_MUTEX_MUST_UNLOCK(mutex);

        switch (pwm.type) {
        case DP_PROJECT_WORKER_MESSAGE_STOP:
            running = false;
            break;
        case DP_PROJECT_WORKER_MESSAGE_PROJECT_OPEN:
            worker_project_open(pw, &pwm.project_open);
            break;
        case DP_PROJECT_WORKER_MESSAGE_PROJECT_CLOSE:
            worker_project_close(pw);
            break;
        case DP_PROJECT_WORKER_MESSAGE_SESSION_OPEN:
            worker_session_open(pw, &pwm.session_open);
            break;
        case DP_PROJECT_WORKER_MESSAGE_SESSION_CLOSE:
            worker_session_close(pw, &pwm.session_close);
            break;
        case DP_PROJECT_WORKER_MESSAGE_MESSAGE_RECORD:
            worker_message_record(pw, &pwm.message_record);
            break;
        default:
            DP_warn("Unhandled project worker message type %d", (int)pwm.type);
            break;
        }
    }
}

static void worker_push(DP_ProjectWorker *pw, DP_ProjectWorkerMessage pwm)
{
    DP_Mutex *mutex = pw->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    *(DP_ProjectWorkerMessage *)DP_queue_push(&pw->queue, sizeof(pwm)) = pwm;
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_SEMAPHORE_MUST_POST(pw->sem);
}

DP_ProjectWorker *DP_project_worker_new(DP_ProjectWorkerEventFn fn, void *user)
{
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
    *pw = (DP_ProjectWorker){mutex, sem, NULL, NULL, DP_QUEUE_NULL, fn, user};
    DP_queue_init(&pw->queue, 1024, sizeof(DP_ProjectWorkerMessage));
    pw->thread = DP_thread_new(worker_run, pw);
    if (!pw->thread) {
        DP_project_close(pw->prj);
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
        worker_push(
            pw, (DP_ProjectWorkerMessage){DP_PROJECT_WORKER_MESSAGE_STOP, {0}});
        DP_thread_free_join(pw->thread);
        DP_queue_dispose(&pw->queue);
        DP_semaphore_free(pw->sem);
        DP_mutex_free(pw->mutex);
        DP_free(pw);
    }
}

void DP_project_worker_project_open(DP_ProjectWorker *pw, const char *path,
                                    unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(path);
    worker_push(pw, (DP_ProjectWorkerMessage){
                        DP_PROJECT_WORKER_MESSAGE_PROJECT_OPEN,
                        .project_open = {DP_strdup(path), flags}});
}

void DP_project_worker_project_close(DP_ProjectWorker *pw)
{
    DP_ASSERT(pw);
    worker_push(pw, (DP_ProjectWorkerMessage){
                        DP_PROJECT_WORKER_MESSAGE_PROJECT_CLOSE, {0}});
}

void DP_project_worker_session_open(DP_ProjectWorker *pw, int source_type,
                                    const char *source_param,
                                    const char *protocol, unsigned int flags)
{
    DP_ASSERT(pw);
    worker_push(pw, (DP_ProjectWorkerMessage){
                        DP_PROJECT_WORKER_MESSAGE_SESSION_OPEN,
                        .session_open = {source_type, DP_strdup(source_param),
                                         DP_strdup(protocol), flags}});
}

void DP_project_worker_session_close(DP_ProjectWorker *pw,
                                     unsigned int flags_to_set)
{
    DP_ASSERT(pw);
    worker_push(
        pw, (DP_ProjectWorkerMessage){DP_PROJECT_WORKER_MESSAGE_SESSION_CLOSE,
                                      .session_close = {flags_to_set}});
}

void DP_project_worker_message_record_noinc(DP_ProjectWorker *pw,
                                            DP_Message *msg, unsigned int flags)
{
    DP_ASSERT(pw);
    DP_ASSERT(msg);
    worker_push(
        pw, (DP_ProjectWorkerMessage){DP_PROJECT_WORKER_MESSAGE_MESSAGE_RECORD,
                                      .message_record = {msg, flags}});
}

void DP_project_worker_message_record_inc(DP_ProjectWorker *pw, DP_Message *msg,
                                          unsigned int flags)
{
    DP_ASSERT(pw);
    DP_project_worker_message_record_noinc(pw, DP_message_incref(msg), flags);
}
