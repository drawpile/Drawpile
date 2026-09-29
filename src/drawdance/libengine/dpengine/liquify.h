// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_LIQUIFY_H
#define DPENGINE_LIQUIFY_H
#include <dpcommon/common.h>

typedef struct DP_DrawContext DP_DrawContext;


typedef struct DP_Liquify DP_Liquify;
typedef struct DP_LiquifyState DP_LiquifyState;
typedef struct DP_LiquifyTransformer DP_LiquifyTransformer;

typedef enum DP_LiquifyOpType {
    DP_LIQUIFY_OP_TYPE_MOVE,
    DP_LIQUIFY_OP_TYPE_SCALE,
    DP_LIQUIFY_OP_TYPE_ROTATE,
    DP_LIQUIFY_OP_TYPE_SMOOTHE,
    DP_LIQUIFY_OP_TYPE_ERASE,
} DP_LiquifyOpType;

typedef struct DP_LiquifyOpMoveParams {
    float dx;
    float dy;
} DP_LiquifyOpMoveParams;

typedef struct DP_LiquifyOpScaleParams {
    float amount;
} DP_LiquifyOpScaleParams;

typedef struct DP_LiquifyOpRotateParams {
    float angle;
} DP_LiquifyOpRotateParams;

typedef struct DP_LiquifyOpSmootheParams {
    float amount;
    float kernel_radius;
} DP_LiquifyOpSmootheParams;

typedef struct DP_LiquifyOpEraseParams {
    float amount;
} DP_LiquifyOpEraseParams;

typedef struct DP_LiquifyOpParams {
    DP_LiquifyOpType type;
    float x;
    float y;
    float radius;
    union {
        DP_LiquifyOpMoveParams move;
        DP_LiquifyOpScaleParams scale;
        DP_LiquifyOpRotateParams rotate;
        DP_LiquifyOpSmootheParams smoothe;
        DP_LiquifyOpEraseParams erase;
    } DP_ANONYMOUS(op);
} DP_LiquifyOpParams;


DP_Liquify *DP_liquify_new(int mask_x, int mask_y, int mask_width,
                           int mask_height,
                           void (*fill_mask)(void *, unsigned char *),
                           void *user);

DP_Liquify *DP_liquify_incref(DP_Liquify *l);
DP_Liquify *DP_liquify_incref_nullable(DP_Liquify *l_or_null);

void DP_liquify_decref(DP_Liquify *l);
void DP_liquify_decref_nullable(DP_Liquify *l_or_null);

int DP_liquify_refcount(DP_Liquify *l);

DP_LiquifyState *DP_liquify_current_state_inc(DP_Liquify *l);

uint32_t *DP_liquify_dump(DP_Liquify *l, int *out_width, int *out_height);

// Must only be called from one thread at a time!
bool DP_liquify_op(DP_Liquify *l, DP_DrawContext *dc,
                   const DP_LiquifyOpParams *params);


DP_LiquifyState *DP_liquify_state_incref(DP_LiquifyState *ls);
DP_LiquifyState *DP_liquify_state_incref_nullable(DP_LiquifyState *ls_or_null);

void DP_liquify_state_decref(DP_LiquifyState *ls);
void DP_liquify_state_decref_nullable(DP_LiquifyState *ls_or_null);

int DP_liquify_state_refcount(DP_LiquifyState *ls);

void DP_liquify_state_apply(DP_LiquifyState *ls);


DP_LiquifyTransformer *DP_liquify_transformer_new(int source_x, int source_y,
                                                  int source_width,
                                                  int source_height,
                                                  const uint32_t *source_data);

DP_LiquifyTransformer *
DP_liquify_transformer_incref(DP_LiquifyTransformer *ltr);
DP_LiquifyTransformer *
DP_liquify_transformer_incref_nullable(DP_LiquifyTransformer *ltr_or_null);

void DP_liquify_transformer_decref(DP_LiquifyTransformer *ltr);
void DP_liquify_transformer_decref_nullable(DP_LiquifyTransformer *ltr_or_null);

int DP_liquify_transformer_refcount(DP_LiquifyTransformer *ltr);

// Not thread-safe! Returns whether the image changed.
bool DP_liquify_transformer_apply(DP_LiquifyTransformer *ltr,
                                  DP_LiquifyState *ls_or_null,
                                  int interpolation);

// The data is mutable and will be invalidated when applying a new state. Make a
// copy of it. Returns true if there is a target image, false if there is not.
// The latter is the case for null transforms.
bool DP_liquify_transformer_target_image(DP_LiquifyTransformer *ltr, int *out_x,
                                         int *out_y, int *out_width,
                                         int *out_height,
                                         const uint32_t **out_data);


#endif
