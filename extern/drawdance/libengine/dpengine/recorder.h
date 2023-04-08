/*
 * Copyright (C) 2022 askmeaboufoom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPMSG_RECORDER_H
#define DPMSG_RECORDER_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


typedef struct DP_Recorder DP_Recorder;

typedef enum DP_RecorderType {
    DP_RECORDER_TYPE_BINARY,
    DP_RECORDER_TYPE_TEXT,
} DP_RecorderType;

typedef long long (*DP_RecorderGetTimeMsFn)(void *user);


DP_Recorder *DP_recorder_new_inc(DP_RecorderType type,
                                 DP_CanvasState *cs_or_null,
                                 DP_RecorderGetTimeMsFn get_time_fn,
                                 void *get_time_user, DP_Output *output);

void DP_recorder_free_join(DP_Recorder *r);

DP_RecorderType DP_recorder_type(DP_Recorder *r);

void DP_recorder_message_push_initial_inc(DP_Recorder *r, int count,
                                          DP_Message *(*get)(void *, int),
                                          void *user);

bool DP_recorder_message_push_inc(DP_Recorder *r, DP_Message *msg);


#endif
