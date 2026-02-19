// SPDX-License-Identifier: GPL-3.0-or-later
#include "process.h"

bool DP_process_supported(void)
{
    return false;
}

DP_Process *DP_process_new(DP_UNUSED int argc, DP_UNUSED const char **argv)
{
    DP_error_set("Not supported");
    return NULL;
}

int DP_process_free_close_wait(DP_UNUSED DP_Process *p)
{
    return -1;
}

void DP_process_kill(DP_UNUSED DP_Process *p)
{
    // Nothing to do.
}

long long DP_process_pid(DP_UNUSED DP_Process *p)
{
    return 0LL;
}

bool DP_process_running(DP_UNUSED DP_Process *p)
{
    return false;
}

bool DP_process_write(DP_UNUSED DP_Process *p, DP_UNUSED size_t len,
                      DP_UNUSED const unsigned char *data)
{
    return false;
}

void DP_process_close(DP_UNUSED DP_Process *p)
{
    // Nothing to do.
}

bool DP_process_wait(DP_UNUSED DP_Process *p, DP_UNUSED int msec)
{
    return false;
}
