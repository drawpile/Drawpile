// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_IMAGE_PNG_H
#define DPIMPEX_IMAGE_PNG_H
#include <dpcommon/common.h>

typedef struct DP_Image DP_Image;
typedef struct DP_Input DP_Input;
typedef struct DP_Output DP_Output;
typedef union DP_Pixel8 DP_Pixel8;
typedef union DP_UPixel8 DP_UPixel8;


DP_Image *DP_image_png_read(DP_Input *input);

bool DP_image_png_write(DP_Output *output, int width, int height,
                        DP_Pixel8 *pixels);

bool DP_image_png_write_unpremultiplied(DP_Output *output, int width, int height,
                         DP_UPixel8 *pixels);


#endif
