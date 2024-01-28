// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_USER_CURSORS_H
#define DPENGINE_USER_CURSORS_H
#include <dpcommon/common.h>

#define DP_USER_CURSOR_COUNT        256
#define DP_USER_CURSOR_SMOOTH_COUNT 8

#define DP_USER_CURSOR_FLAG_NONE        0x0u
#define DP_USER_CURSOR_FLAG_VALID       0x1u
#define DP_USER_CURSOR_FLAG_INTERPOLATE 0x2u
#define DP_USER_CURSOR_FLAG_PEN_UP      0x4u
#define DP_USER_CURSOR_FLAG_PEN_DOWN    0x8u

typedef struct DP_UserCursor {
    unsigned int flags;
    unsigned int context_id;
    int layer_id;
    int x, y;
} DP_UserCursor;

typedef struct DP_UserCursorState {
    uint8_t flags;
    uint8_t smooth_index;
    uint8_t smooth_count;
    int layer_id;
    float last_r2;
    int xs[DP_USER_CURSOR_SMOOTH_COUNT];
    int ys[DP_USER_CURSOR_SMOOTH_COUNT];
} DP_UserCursorState;

typedef struct DP_UserCursors {
    int count;
    uint8_t user_ids[DP_USER_CURSOR_COUNT];
    bool active_by_user[DP_USER_CURSOR_COUNT];
    DP_UserCursorState states[DP_USER_CURSOR_COUNT];
} DP_UserCursors;

typedef struct DP_EffectiveUserCursorState {
    unsigned int flags;
    int layer_id;
    int x, y;
} DP_EffectiveUserCursorState;

typedef struct DP_EffectiveUserCursors {
    int count;
    uint8_t user_ids[DP_USER_CURSOR_COUNT];
    bool active_by_user[DP_USER_CURSOR_COUNT];
    DP_EffectiveUserCursorState states[DP_USER_CURSOR_COUNT];
} DP_EffectiveUserCursors;

typedef struct DP_UserCursorBuffer {
    int count;
    DP_UserCursor cursors[DP_USER_CURSOR_COUNT];
} DP_UserCursorBuffer;


void DP_user_cursors_init(DP_UserCursors *ucs);

void DP_user_cursors_activate(DP_UserCursors *ucs, unsigned int context_id);

void DP_user_cursors_move(DP_UserCursors *ucs, unsigned int context_id,
                          int layer_id, int x, int y);

void DP_user_cursors_move_smooth(DP_UserCursors *ucs, unsigned int context_id,
                                 int layer_id, int x, int y, float radius);

void DP_user_cursors_pen_up(DP_UserCursors *ucs, unsigned int context_id);


void DP_effective_user_cursors_init(DP_EffectiveUserCursors *eucs);

void DP_effective_user_cursors_apply(DP_EffectiveUserCursors *eucs,
                                     DP_UserCursors *ucs);

void DP_effective_user_cursors_retrieve(DP_EffectiveUserCursors *eucs,
                                        DP_UserCursorBuffer *ucb);


#endif
