// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_DAB_COST_H
#define DPENGINE_DAB_COST_H
#include <dpcommon/common.h>

double DP_dab_cost_pixel(bool indirect, int blend_mode);

double DP_dab_cost_pixel_square(bool indirect, int blend_mode);

double DP_dab_cost_classic(bool indirect, int blend_mode);

double DP_dab_cost_mypaint(bool indirect, uint8_t lock_alpha, uint8_t colorize,
                           uint8_t posterize);

#endif
