// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPMSG_LOCAL_MATCH_H
#define DPMSG_LOCAL_MATCH_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;

DP_Message *DP_msg_local_match_make(DP_Message *msg,
                                    bool disguise_as_put_image);

bool DP_msg_local_match_is_local_match(DP_Message *msg);

bool DP_msg_local_match_matches(DP_Message *msg, DP_Message *local_match_msg);

#endif
