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
#ifndef DPENGINE_SAVE_H
#define DPENGINE_SAVE_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;


typedef struct DP_SaveFormat {
    const char *title;
    const char **extensions; // Terminated by a NULL element.
} DP_SaveFormat;

// Returns supported formats for saving, terminated by a {NULL, NULL} element.
const DP_SaveFormat *DP_save_supported_formats(void);


typedef enum DP_SaveImageType {
    DP_SAVE_IMAGE_GUESS,
    DP_SAVE_IMAGE_ORA,
    DP_SAVE_IMAGE_PNG,
    DP_SAVE_IMAGE_JPEG,
} DP_SaveImageType;

typedef enum DP_SaveResult {
    DP_SAVE_RESULT_SUCCESS,
    DP_SAVE_RESULT_BAD_ARGUMENTS,
    DP_SAVE_RESULT_NO_EXTENSION,
    DP_SAVE_RESULT_UNKNOWN_FORMAT,
    DP_SAVE_RESULT_FLATTEN_ERROR,
    DP_SAVE_RESULT_OPEN_ERROR,
    DP_SAVE_RESULT_WRITE_ERROR,
    DP_SAVE_RESULT_INTERNAL_ERROR,
    DP_SAVE_RESULT_CANCEL,
} DP_SaveResult;

DP_SaveResult DP_save(DP_CanvasState *cs, DP_DrawContext *dc,
                      DP_SaveImageType type, const char *path);


typedef bool (*DP_SaveAnimationProgressFn)(void *user, double progress);

DP_SaveResult DP_save_animation_frames(DP_CanvasState *cs, const char *path,
                                       DP_SaveAnimationProgressFn progress_fn,
                                       void *user);

DP_SaveResult DP_save_animation_gif(DP_CanvasState *cs, const char *path,
                                    DP_SaveAnimationProgressFn progress_fn,
                                    void *user);


#endif
