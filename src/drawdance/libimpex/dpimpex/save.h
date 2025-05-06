// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_SAVE_H
#define DPIMPEX_SAVE_H
#include <dpcommon/common.h>
#include <dpengine/save_enums.h>

typedef struct DP_Annotation DP_Annotation;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Rect DP_Rect;
typedef struct DP_ViewModeFilter DP_ViewModeFilter;


typedef struct DP_SaveFormat {
    const char *title;
    const char **extensions; // Terminated by a NULL element.
} DP_SaveFormat;

// Returns supported formats for saving, terminated by a {NULL, NULL} element.
const DP_SaveFormat *DP_save_supported_formats(void);

typedef bool (*DP_SaveBakeAnnotationFn)(void *user, DP_Annotation *a,
                                        unsigned char *out);


DP_SaveImageType DP_save_image_type_guess(const char *path);

const char *DP_save_image_type_name(DP_SaveImageType type);

int DP_save_image_type_max_dimension(DP_SaveImageType type);

// Calls DP_error_set with an appropriate error message if out of bounds.
bool DP_save_check_width_height(int width, int height, int max_dimension);
bool DP_save_check_dimensions(DP_CanvasState *cs, int max_dimension);
bool DP_save_check_type_dimensions(DP_CanvasState *cs, DP_SaveImageType type);

bool DP_save_image_type_is_flat_image(DP_SaveImageType type);

DP_SaveResult DP_save(DP_CanvasState *cs, DP_DrawContext *dc,
                      DP_SaveImageType type, const char *path,
                      const DP_ViewModeFilter *vmf_or_null,
                      DP_SaveBakeAnnotationFn bake_annotation, void *user);


typedef bool (*DP_SaveAnimationProgressFn)(void *user, double progress);

// To use the default values from the canvas state for the below functions, crop
// can be NULL, start, end_inclusive and framerate can be -1.
DP_SaveResult DP_save_animation_frames(DP_CanvasState *cs, DP_DrawContext *dc,
                                       const char *path, DP_Rect *crop,
                                       int width, int height, int interpolation,
                                       int start, int end_inclusive,
                                       DP_SaveAnimationProgressFn progress_fn,
                                       void *user);

DP_SaveResult DP_save_animation_zip(DP_CanvasState *cs, DP_DrawContext *dc,
                                    const char *path, DP_Rect *crop, int width,
                                    int height, int interpolation, int start,
                                    int end_inclusive,
                                    DP_SaveAnimationProgressFn progress_fn,
                                    void *user);


#endif
