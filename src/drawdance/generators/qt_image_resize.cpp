/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <cstdio>
#include <QImage>
#include "../libimpex/test/resize_image.h"

static QImage generate_base_image()
{
    QImage base(WIDTH, HEIGHT, QImage::Format_ARGB32_Premultiplied);
    double wf = WIDTH;
    double hf = HEIGHT;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int b = static_cast<int>(static_cast<double>(y) / hf * 255.0);
            int g = static_cast<int>(static_cast<double>(x) / wf * 255.0);
            int r = 255;
            int a = 255;
            base.setPixelColor(x, y, QColor(r, g, b, a));
        }
    }
    return base;
}

static void generate_resize_test_image(const QImage &base, const QString &path, int x, int y, int w, int h)
{
    QImage resized = base.copy(x, y, w, h);
    if (!resized.save(path)) {
        std::fprintf(stderr, "Couldn't save %s\n", path.toStdString().c_str());
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stdout, "Usage: %s OUTPUT_DIRECTORY\n", argc == 0 ? "qt_image_resize" : argv[0]);
        return 2;
    }

    QImage base = generate_base_image();
    for (const ResizeTestCase &rtc : resize_test_cases) {
        QString path = QString("%1/%2.png").arg(argv[1]).arg(rtc.name);
        generate_resize_test_image(base, path, rtc.x, rtc.y, WIDTH + rtc.dw, HEIGHT + rtc.dh);
    }

    return 0;
}
