// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_PROCESS_H
#define DPIMPEX_PROCESS_H
#include <dpcommon/common.h>

typedef struct DP_Process DP_Process;

// Returns whether running processes is supported on this platform. If not,
// attempting to create a process always fails.
bool DP_process_supported(void);

// Opens a process with the given arguments that can be written to and writes
// its stdout and stderr to the parent process' channels. The program to run
// must be in argv[0].
DP_Process *DP_process_new(int argc, const char **argv);

// Closes the process' write end, waits for it to exit and returns the exit
// code, which may be -1 if the given process was null or the process didn't
// give an exit code for some reason.
int DP_process_free_close_wait(DP_Process *p);

void DP_process_kill(DP_Process *p);

long long DP_process_pid(DP_Process *p);

bool DP_process_running(DP_Process *p);

bool DP_process_write(DP_Process *p, size_t len, const unsigned char *data);

void DP_process_close(DP_Process *p);

bool DP_process_wait(DP_Process *p, int msec);

#endif
