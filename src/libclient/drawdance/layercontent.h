// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_LAYERCONTENT_H
#define DRAWDANCE_LAYERCONTENT_H

#include <QColor>
#include <QImage>

class QRect;
struct DP_LayerContent;

namespace drawdance {

class LayerContent final {
public:
    static LayerContent null();
    static LayerContent inc(DP_LayerContent *lc);
    static LayerContent noinc(DP_LayerContent *lc);

    LayerContent(const LayerContent &other);
    LayerContent(LayerContent &&other);

    LayerContent &operator=(const LayerContent &other);
    LayerContent &operator=(LayerContent &&other);

    ~LayerContent();

    DP_LayerContent *get() const;

    bool isNull() const;

    QColor sampleColorAt(
        uint16_t *stampBuffer, int x, int y, int diameter, bool opaque,
        int &lastDiameter) const;

    bool isTransparentPixelAt(int x, int y) const;

    QImage toImage(const QRect &rect) const;

    QImage toImageMask(const QRect &rect, const QColor &color) const;

private:
    explicit LayerContent(DP_LayerContent *lc);

    DP_LayerContent *m_data;
};

}

#endif
