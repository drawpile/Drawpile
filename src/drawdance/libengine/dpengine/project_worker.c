// SPDX-License-Identifier: GPL-3.0-or-later
#include "project_worker.h"
#include "project.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>


struct DP_ProjectWorker {
    DP_Mutex *mutex;
    DP_Thread *thread;
    DP_Queue queue;
};

typedef enum DP_ProjectWorkerMessageType {
    DP_PROJECT_WORKER_MESSAGE_PROJECT_OPEN,
    DP_PROJECT_WORKER_MESSAGE_PROJECT_CLOSE,
} DP_ProjectWorkerMessageType;

typedef struct DP_ProjectWorkerMessageProjectOpen {
    const char *path;
    unsigned int flags;
    DP_ProjectWorkerOpenResultFn fn;
    void *user;
} DP_ProjectWorkerMessageProjectOpen;

typedef struct DP_ProjectWorkerMessage {
    DP_ProjectWorkerMessageType type;
    union {
        DP_ProjectWorkerMessageProjectOpen project_open;
    };
} DP_ProjectWorkerMessage;


DP_ProjectWorker *DP_project_worker_new(void);

void DP_project_worker_free_join(DP_ProjectWorker *pw);
