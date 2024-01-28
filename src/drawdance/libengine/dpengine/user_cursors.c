// SPDX-License-Identifier: GPL-3.0-or-later
#include "user_cursors.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <math.h>

static void init_user_cursor_state(DP_UserCursorState *state)
{
    state->flags = DP_USER_CURSOR_FLAG_NONE;
    state->layer_id = 0;
    for (int i = 0; i < DP_USER_CURSOR_SMOOTH_COUNT; ++i) {
        state->xs[i] = 0;
        state->ys[i] = 0;
        state->last_r2 = 0.0f;
    }
}

void DP_user_cursors_init(DP_UserCursors *ucs)
{
    DP_ASSERT(ucs);
    ucs->count = 0;
    for (int i = 0; i < DP_USER_CURSOR_COUNT; ++i) {
        ucs->active_by_user[i] = false;
        ucs->user_ids[i] = 0;
        init_user_cursor_state(&ucs->states[i]);
    }
}

void DP_user_cursors_activate(DP_UserCursors *ucs, unsigned int context_id)
{
    DP_ASSERT(ucs);
    DP_ASSERT(context_id < DP_USER_CURSOR_COUNT);
    if (!ucs->active_by_user[context_id]) {
        ucs->active_by_user[context_id] = true;
        ucs->user_ids[ucs->count++] = DP_uint_to_uint8(context_id);
    }
}

void DP_user_cursors_move(DP_UserCursors *ucs, unsigned int context_id,
                          int layer_id, int x, int y)
{
    DP_ASSERT(ucs);
    DP_ASSERT(context_id < DP_USER_CURSOR_COUNT);
    DP_UserCursorState *state = &ucs->states[context_id];
    state->flags |= DP_USER_CURSOR_FLAG_VALID | DP_USER_CURSOR_FLAG_PEN_DOWN;
    state->flags &= (uint8_t)~DP_USER_CURSOR_FLAG_INTERPOLATE;
    state->layer_id = layer_id;
    state->smooth_count = 0;
    state->smooth_index = 0;
    state->xs[0] = x;
    state->ys[0] = y;
}

void DP_user_cursors_move_smooth(DP_UserCursors *ucs, unsigned int context_id,
                                 int layer_id, int x, int y, float radius)
{
    DP_ASSERT(ucs);
    DP_ASSERT(context_id < DP_USER_CURSOR_COUNT);
    DP_UserCursorState *state = &ucs->states[context_id];
    state->layer_id = layer_id;
    state->flags |= DP_USER_CURSOR_FLAG_VALID | DP_USER_CURSOR_FLAG_PEN_DOWN;

    float r2 = radius * radius;
    int smooth_count = state->smooth_count;
    int i;
    if (state->smooth_count == 0) {
        state->flags &= (uint8_t)~DP_USER_CURSOR_FLAG_INTERPOLATE;
        state->smooth_index = 0;
        state->smooth_count = 1;
        i = 0;
    }
    else if (smooth_count < DP_USER_CURSOR_SMOOTH_COUNT) {
        ++state->smooth_count;
        i = ++state->smooth_index;
    }
    else {
        int prev = state->smooth_index;
        uint8_t smooth_index =
            ++state->smooth_index % DP_USER_CURSOR_SMOOTH_COUNT;
        state->smooth_index = smooth_index;
        i = smooth_index;
        if (!(state->flags & DP_USER_CURSOR_FLAG_INTERPOLATE)) {
            float dx = DP_int_to_float(x - state->xs[prev]);
            float dy = DP_int_to_float(y - state->ys[prev]);
            float distance_squared = dx * dx + dy * dy;
            if (distance_squared > 4.0f
                && distance_squared > (r2 + state->last_r2) * 2.0f) {
                state->flags |= DP_USER_CURSOR_FLAG_INTERPOLATE;
            }
        }
    }

    state->xs[i] = x;
    state->ys[i] = y;
    state->last_r2 = r2;
}

void DP_user_cursors_pen_up(DP_UserCursors *ucs, unsigned int context_id)
{
    DP_ASSERT(ucs);
    DP_UserCursorState *state = &ucs->states[context_id];
    state->flags |= DP_USER_CURSOR_FLAG_PEN_UP;
    state->smooth_count = 0;
}


void DP_effective_user_cursors_init(DP_EffectiveUserCursors *eucs)
{
    DP_ASSERT(eucs);
    eucs->count = 0;
    for (int i = 0; i < DP_USER_CURSOR_COUNT; ++i) {
        eucs->active_by_user[i] = false;
        eucs->user_ids[i] = 0;
        eucs->states[i] =
            (DP_EffectiveUserCursorState){DP_USER_CURSOR_FLAG_NONE, 0, 0, 0};
    }
}

void DP_effective_user_cursors_apply(DP_EffectiveUserCursors *eucs,
                                     DP_UserCursors *ucs)
{
    DP_ASSERT(eucs);
    DP_ASSERT(ucs);
    int count = ucs->count;
    ucs->count = 0;
    for (int i = 0; i < count; ++i) {
        unsigned int context_id = ucs->user_ids[i];
        DP_UserCursorState *state = &ucs->states[context_id];
        ucs->active_by_user[context_id] = false;

        if (!eucs->active_by_user[context_id]) {
            eucs->active_by_user[context_id] = true;
            eucs->user_ids[eucs->count++] = DP_uint_to_uint8(context_id);
        }
        DP_EffectiveUserCursorState *effective_state =
            &eucs->states[context_id];
        unsigned int flags = state->flags;
        effective_state->flags |= flags;
        effective_state->layer_id = state->layer_id;

        int smooth_count = state->smooth_count;
        if (smooth_count == 0) {
            int smooth_index = state->smooth_index;
            effective_state->x = state->xs[smooth_index];
            effective_state->y = state->ys[smooth_index];
        }
        else {
            long long x = state->xs[0];
            long long y = state->ys[0];
            for (int j = 1; j < smooth_count; ++j) {
                x += state->xs[j];
                y += state->ys[j];
            }
            long long smooth_count_ll = smooth_count;
            effective_state->x = DP_llong_to_int(x / smooth_count_ll);
            effective_state->y = DP_llong_to_int(y / smooth_count_ll);
        }

        state->flags &= (uint8_t) ~(DP_USER_CURSOR_FLAG_PEN_UP
                                    | DP_USER_CURSOR_FLAG_PEN_DOWN);
    }
}

void DP_effective_user_cursors_retrieve(DP_EffectiveUserCursors *eucs,
                                        DP_UserCursorBuffer *ucb)
{
    DP_ASSERT(eucs);
    DP_ASSERT(ucb);
    int count = eucs->count;
    eucs->count = 0;
    for (int i = 0; i < count; ++i) {
        unsigned int context_id = eucs->user_ids[i];
        DP_EffectiveUserCursorState *state = &eucs->states[context_id];
        eucs->active_by_user[context_id] = false;
        ucb->cursors[i] = (DP_UserCursor){state->flags, context_id,
                                          state->layer_id, state->x, state->y};
        state->flags = DP_USER_CURSOR_FLAG_NONE;
    }
    ucb->count = count;
}
