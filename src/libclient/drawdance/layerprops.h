// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_LAYER_PROPS_H
#define DRAWDANCE_LAYER_PROPS_H

#include <QString>

struct DP_LayerProps;

namespace drawdance {

class LayerPropsList;

class LayerProps final {
public:
    static LayerProps null();
    static LayerProps inc(DP_LayerProps *lp);
    static LayerProps noinc(DP_LayerProps *lp);

    LayerProps(const LayerProps &other);
    LayerProps(LayerProps &&other);

    LayerProps &operator=(const LayerProps &other);
    LayerProps &operator=(LayerProps &&other);

    ~LayerProps();

    DP_LayerProps *get() const;

    bool isNull() const;

    int id() const;
    QString title() const;
    uint16_t opacity() const;
    int blendMode() const;
    bool hidden() const;
    bool censored() const;
    bool isolated() const;
    bool clip() const;
    uint16_t sketchOpacity() const;
    uint32_t sketchTint() const;

    bool isGroup(drawdance::LayerPropsList *outChildren);

private:
    explicit LayerProps(DP_LayerProps *lp);

    DP_LayerProps *m_data;
};

}

#endif
