// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "image_jpeg.h"
#include "image_png.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/image.h>
#include <dpengine/pixels.h>
}
#include <dpcommon/platform_qt.h>
#include <QIODevice>
#include <QImage>
#include <QImageReader>


extern "C" void DP_image_impex_init(void)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QImageReader::setAllocationLimit(0);
#endif
}


class DP_InputDevice : public QIODevice {
  public:
    explicit DP_InputDevice(DP_Input *input) : QIODevice{}, m_input{input}
    {
        open(QIODevice::ReadOnly);
    }

    qint64 size() const override
    {
        bool error;
        size_t length = DP_input_length(m_input, &error);
        return error ? 0 : qint64(length);
    }

  protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        bool error;
        size_t read = DP_input_read(m_input, data, size_t(maxSize), &error);
        if (error) {
            setErrorString(QString::fromUtf8(DP_error()));
            return -1;
        }
        else {
            return qint64(read);
        }
    }

    qint64 writeData(const char *, qint64) override
    {
        setErrorString(QStringLiteral("Can't write to an input"));
        return -1;
    }

  private:
    DP_Input *m_input;
};

class DP_OutputDevice : public QIODevice {
  public:
    explicit DP_OutputDevice(DP_Output *output) : QIODevice{}, m_output{output}
    {
        open(DP_QT_WRITE_FLAGS);
    }

  protected:
    qint64 readData(char *, qint64) override
    {
        setErrorString(QStringLiteral("Can't read from an output"));
        return -1;
    }

    qint64 writeData(const char *data, qint64 len) override
    {
        bool ok = DP_output_write(m_output, data, size_t(len));
        if (ok) {
            return len;
        }
        else {
            setErrorString(QString::fromUtf8(DP_error()));
            return -1;
        }
    }

  private:
    DP_Output *m_output;
};


static bool load_qimage_from_qiodevice(QIODevice *dev, const char *format,
                                       QImage &qi, int &width, int &height)
{
    if (qi.load(dev, format)) {
        width = qi.width();
        height = qi.height();
        return width > 0 && height > 0;
    }
    else {
        return false;
    }
}

static bool load_qimage(DP_Input *input, const char *format, QImage &qi,
                        int &width, int &height)
{
    QIODevice *dev = DP_input_qiodevice(input);
    if (dev) {
        return load_qimage_from_qiodevice(dev, format, qi, width, height);
    }
    else {
        DP_InputDevice wrapper(input);
        return load_qimage_from_qiodevice(&wrapper, format, qi, width, height);
    }
}

static DP_Image *read_image(DP_Input *input, const char *format)
{
    unsigned int error_count = DP_error_count();
    QImage qi;
    int width, height;
    if (load_qimage(input, format, qi, width, height)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        qi.convertTo(QImage::Format_ARGB32_Premultiplied);
#else
        qi = qi.convertToFormat(QImage::Format_ARGB32_Premultiplied);
#endif
        DP_Image *img = DP_image_new(width, height);
        DP_Pixel8 *pixels = DP_image_pixels(img);
        size_t line_length = DP_int_to_size(width) * sizeof(*pixels);
        for (int i = 0; i < height; ++i) {
            memcpy(pixels + i * width, qi.constScanLine(i), line_length);
        }
        return img;
    }
    else {
        if (DP_error_count_since(error_count) == 0) {
            DP_error_set("Could not load %s image", format);
        }
        return nullptr;
    }
}

extern "C" DP_Image *DP_image_png_read(DP_Input *input)
{
    return read_image(input, "PNG");
}

extern "C" DP_Image *DP_image_jpeg_read(DP_Input *input)
{
    return read_image(input, "JPEG");
}


static bool write_image_to_qiodevice(QIODevice *dev, int width, int height,
                                     uchar *pixels, const char *format,
                                     int quality, QImage::Format imageFormat,
                                     unsigned int error_count)
{
    QImage qi(pixels, width, height, width * 4, imageFormat);
    if (qi.save(dev, format, quality)) {
        return true;
    }
    else {
        if (DP_error_count_since(error_count) == 0) {
            DP_error_set("Could not save %s image", format);
        }
        return false;
    }
}

static bool write_image(DP_Output *output, int width, int height, uchar *pixels,
                        const char *format, int quality,
                        QImage::Format imageFormat)
{
    unsigned int error_count = DP_error_count();
    QIODevice *dev = DP_output_qiodevice(output);
    bool ok;
    if (dev) {
        ok = write_image_to_qiodevice(dev, width, height, pixels, format,
                                      quality, imageFormat, error_count);
    }
    else {
        DP_OutputDevice wrapper(output);
        ok = write_image_to_qiodevice(&wrapper, width, height, pixels, format,
                                      quality, imageFormat, error_count);
    }
    return ok ? DP_output_flush(output) : false;
}

extern "C" bool DP_image_png_write(DP_Output *output, int width, int height,
                                   DP_Pixel8 *pixels)
{
    return write_image(output, width, height, reinterpret_cast<uchar *>(pixels),
                       "PNG", -1, QImage::Format_ARGB32_Premultiplied);
}

extern "C" bool DP_image_png_write_unpremultiplied(DP_Output *output, int width,
                                                   int height,
                                                   DP_UPixel8 *pixels)
{
    return write_image(output, width, height, reinterpret_cast<uchar *>(pixels),
                       "PNG", -1, QImage::Format_ARGB32);
}

bool DP_image_jpeg_write(DP_Output *output, int width, int height,
                         DP_Pixel8 *pixels, int quality)
{
    return write_image(output, width, height, reinterpret_cast<uchar *>(pixels),
                       "JPEG", quality, QImage::Format_ARGB32_Premultiplied);
}
