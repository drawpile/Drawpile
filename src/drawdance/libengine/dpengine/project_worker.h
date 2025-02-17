// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PROJECT_WORKER_H
#define DPENGINE_PROJECT_WORKER_H
#include <dpcommon/common.h>

typedef struct DP_ProjectWorker DP_ProjectWorker;

typedef void (*DP_ProjectWorkerOpenResultFn)(void *user, int error,
                                             bool was_busy);

typedef void (*DP_ProjectWorkerCloseResultFn)(void *user, int result);


DP_ProjectWorker *DP_project_worker_new(void);

void DP_project_worker_free_join(DP_ProjectWorker *pw);


void DP_project_worker_open(DP_ProjectWorker *pw, const char *path,
                            unsigned int flags, DP_ProjectWorkerOpenResultFn fn,
                            void *user);

void DP_project_worker_close(DP_ProjectWorker *pw,
                             DP_ProjectWorkerCloseResultFn fn, void *user);


#endif
