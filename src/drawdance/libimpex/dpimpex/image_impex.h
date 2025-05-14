// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_IMAGE_H
#define DPIMPEX_IMAGE_H
#include <dpcommon/common.h>
#include <dpengine/image.h>

typedef struct DP_Image DP_Image;
typedef struct DP_Input DP_Input;
typedef struct DP_Output DP_Output;


// Should be called on application startup to disable the image reader
// allocation limit in Qt6. Other implementations do nothing here.
void DP_image_impex_init(void);

DP_Image *DP_image_new_from_file(DP_Input *input, DP_ImageFileType type,
                                 DP_ImageFileType *out_type);

DP_Image *DP_image_read_png(DP_Input *input);
DP_Image *DP_image_read_jpeg(DP_Input *input);
DP_Image *DP_image_read_jpeg(DP_Input *input);

bool DP_image_write_png(DP_Image *img, DP_Output *output) DP_MUST_CHECK;
bool DP_image_write_jpeg_quality(DP_Image *img, DP_Output *output,
                                 int quality) DP_MUST_CHECK;
bool DP_image_write_jpeg(DP_Image *img, DP_Output *output) DP_MUST_CHECK;
bool DP_image_write_webp(DP_Image *img, DP_Output *output) DP_MUST_CHECK;


#endif
