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

bool DP_psd_read_utf16be_layer_title(DP_TransientLayerProps *tlp,
                                     const uint16_t *be);

DP_SaveResult DP_save_psd(DP_CanvasState *cs, const char *path,
                          DP_DrawContext *dc);

#endif /* DPENGINE_RUST_H */
