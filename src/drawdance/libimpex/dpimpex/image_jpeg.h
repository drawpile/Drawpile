// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_IMAGE_JPEG_H
#define DPIMPEX_IMAGE_JPEG_H
#include <dpcommon/common.h>

typedef struct DP_Image DP_Image;
typedef struct DP_Input DP_Input;
typedef struct DP_Output DP_Output;
typedef union DP_Pixel8 DP_Pixel8;


DP_Image *DP_image_jpeg_read(DP_Input *input);

bool DP_image_jpeg_write(DP_Output *output, int width, int height,
                         DP_Pixel8 *pixels, int quality);


#endif
