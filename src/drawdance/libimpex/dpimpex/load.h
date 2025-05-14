// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_LOAD_H
#define DPIMPEX_LOAD_H
#include <dpcommon/common.h>
#include <dpengine/load_enums.h>
#include <dpengine/save_enums.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Input DP_Input;
typedef struct DP_Player DP_Player;

#define DP_LOAD_FLAG_NONE          0u
#define DP_LOAD_FLAG_SINGLE_THREAD (1u << 0u)


typedef struct DP_LoadFormat {
    const char *title;
    const char **extensions; // Terminated by a NULL element.
} DP_LoadFormat;

// Returns supported formats for loading, terminated by a {NULL, NULL} element.
const DP_LoadFormat *DP_load_supported_formats(void);


typedef void (*DP_LoadFixedLayerFn)(void *user, int layer_id);

DP_SaveImageType DP_load_guess(const unsigned char *buf, size_t size);

DP_CanvasState *DP_load(DP_DrawContext *dc, const char *path,
                        const char *flat_image_layer_title, unsigned int flags,
                        DP_LoadResult *out_result, DP_SaveImageType *out_type);

DP_CanvasState *DP_load_ora(DP_DrawContext *dc, const char *path,
                            unsigned int flags,
                            DP_LoadFixedLayerFn on_fixed_layer, void *user,
                            DP_LoadResult *out_result);

DP_CanvasState *DP_load_psd(DP_DrawContext *dc, DP_Input *input,
                            DP_LoadResult *out_result);

DP_CanvasState *DP_load_project_canvas(DP_DrawContext *dc, const char *path,
                                       unsigned int flags,
                                       DP_LoadResult *out_result);

DP_Player *DP_load_recording(const char *path, DP_LoadResult *out_result);

DP_Player *DP_load_debug_dump(const char *path, DP_LoadResult *out_result);


#endif
