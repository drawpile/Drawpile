/*
 * Copyright (C) 2022 - 2023 askmeaboutloom
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
#ifndef DPENGINE_LOAD_H
#define DPENGINE_LOAD_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Player DP_Player;


typedef struct DP_LoadFormat {
    const char *title;
    const char **extensions; // Terminated by a NULL element.
} DP_LoadFormat;

// Returns supported formats for loading, terminated by a {NULL, NULL} element.
const DP_LoadFormat *DP_load_supported_formats(void);


typedef enum DP_LoadResult {
    DP_LOAD_RESULT_SUCCESS,
    DP_LOAD_RESULT_BAD_ARGUMENTS,
    DP_LOAD_RESULT_UNKNOWN_FORMAT,
    DP_LOAD_RESULT_OPEN_ERROR,
    DP_LOAD_RESULT_READ_ERROR,
    DP_LOAD_RESULT_BAD_MIMETYPE,
    DP_LOAD_RESULT_RECORDING_INCOMPATIBLE,
    DP_LOAD_RESULT_UNSUPPORTED_PSD_BITS_PER_CHANNEL,
    DP_LOAD_RESULT_UNSUPPORTED_PSD_COLOR_MODE,
    DP_LOAD_RESULT_IMAGE_TOO_LARGE,
    DP_LOAD_RESULT_INTERNAL_ERROR,
} DP_LoadResult;

typedef void (*DP_LoadFixedLayerFn)(void *user, int layer_id);

DP_CanvasState *DP_load(DP_DrawContext *dc, const char *path,
                        const char *flat_image_layer_title,
                        DP_LoadResult *out_result);

DP_CanvasState *DP_load_ora(DP_DrawContext *dc, const char *path,
                            DP_LoadFixedLayerFn on_fixed_layer, void *user,
                            DP_LoadResult *out_result);

DP_CanvasState *DP_load_psd(DP_DrawContext *dc, const char *path,
                            DP_LoadResult *out_result);

DP_Player *DP_load_recording(const char *path, DP_LoadResult *out_result);

DP_Player *DP_load_debug_dump(const char *path, DP_LoadResult *out_result);


#endif
