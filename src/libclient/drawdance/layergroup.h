// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_LAYERGROUP_H
#define DRAWDANCE_LAYERGROUP_H

#include <QImage>

class QRect;
struct DP_LayerGroup;

namespace drawdance {

class LayerProps;

class LayerGroup final {
public:
    static LayerGroup null();
    static LayerGroup inc(DP_LayerGroup *lg);
    static LayerGroup noinc(DP_LayerGroup *lg);

    LayerGroup(const LayerGroup &other);
    LayerGroup(LayerGroup &&other);

    LayerGroup &operator=(const LayerGroup &other);
    LayerGroup &operator=(LayerGroup &&other);

    ~LayerGroup();

    bool isNull() const;

    QImage toImage(const LayerProps &layerProps, const QRect &rect) const;

private:
    explicit LayerGroup(DP_LayerGroup *lg);

    DP_LayerGroup *m_data;
};

}

#endif
