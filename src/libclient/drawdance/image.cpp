extern "C" {
#include <dpengine/image.h>
}

#include "libclient/drawdance/image.h"

static void cleanupImage(void *user)
{
    DP_image_free(static_cast<DP_Image *>(user));
}

static void cleanupPixels8(void *user)
{
    DP_free(static_cast<DP_Pixel8 *>(user));
}

namespace drawdance {

QImage wrapImage(DP_Image *img)
{
    if(img) {
        return QImage{
            reinterpret_cast<const uchar *>(DP_image_pixels(img)),
            DP_image_width(img), DP_image_height(img),
            QImage::Format_ARGB32_Premultiplied, cleanupImage, img};
    } else {
        return QImage{};
    }
}

QImage wrapPixels8(int width, int height, DP_Pixel8 *pixels)
{
    if(width > 0 && height > 0 && pixels) {
        return QImage{
            reinterpret_cast<const uchar *>(pixels), width, height,
            QImage::Format_ARGB32_Premultiplied, cleanupPixels8, pixels};
    } else {
        return QImage{};
    }
}

}
