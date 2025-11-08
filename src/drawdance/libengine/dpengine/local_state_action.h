// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_LOCAL_STATE_ACTION_H
#define DPENGINE_LOCAL_STATE_ACTION_H
#include <stdbool.h>
#include <stdint.h>

typedef enum DP_LocalStateActionType {
    DP_LOCAL_STATE_ACTION_BACKGROUND_COLOR,
    DP_LOCAL_STATE_ACTION_VIEW_MODE,
    DP_LOCAL_STATE_ACTION_ACTIVE,
    DP_LOCAL_STATE_ACTION_LAYER_HIDE,
    DP_LOCAL_STATE_ACTION_LAYER_ENABLE_ALPHA_LOCK,
    DP_LOCAL_STATE_ACTION_LAYER_CENSOR,
    DP_LOCAL_STATE_ACTION_LAYER_ENABLE_SKETCH,
    DP_LOCAL_STATE_ACTION_TRACK_HIDE,
    DP_LOCAL_STATE_ACTION_TRACK_ENABLE_ONION_SKIN,
} DP_LocalStateActionType;

typedef struct DP_LocalStateAction {
    DP_LocalStateActionType type;
    union {
        int id;
        uint32_t color;
        struct {
            int mode;
            bool reveal_censored;
        } view;
        struct {
            int layer_id;
            int frame_index;
        } active;
        struct {
            int id;
            uint16_t opacity;
            uint32_t tint;
        } sketch;
    } data;
} DP_LocalStateAction;

#endif
