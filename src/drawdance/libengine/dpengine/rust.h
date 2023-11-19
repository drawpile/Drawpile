// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DPENGINE_RUST_H
#define DPENGINE_RUST_H

// This file is auto-generated , don't edit it manually.
// To regenerate it, run src/drawdance/generators/cbindgen.bash

#include "rust_preamble.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef void (*DP_LoadOldAnimationSetGroupTitleFn)(DP_TransientLayerProps *tlp,
                                                   int i);

typedef void (*DP_LoadOldAnimationSetTrackTitleFn)(DP_TransientTrack *tt,
                                                   int i);

DP_CanvasState *
DP_load_old_animation(DP_DrawContext *dc, const char *path, int hold_time,
                      int framerate, unsigned int flags,
                      DP_LoadOldAnimationSetGroupTitleFn set_group_title,
                      DP_LoadOldAnimationSetTrackTitleFn set_track_title,
                      DP_LoadResult *out_result);

bool DP_psd_read_utf16be_layer_title(DP_TransientLayerProps *tlp,
                                     const uint16_t *be);

DP_SaveResult DP_save_psd(DP_CanvasState *cs, const char *path,
                          DP_DrawContext *dc);

#endif /* DPENGINE_RUST_H */
