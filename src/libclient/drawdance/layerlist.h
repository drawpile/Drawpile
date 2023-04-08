// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_LAYER_LIST_H
#define DRAWDANCE_LAYER_LIST_H

struct DP_LayerList;

namespace drawdance {

class LayerList final {
public:
    static LayerList inc(DP_LayerList *ll);
    static LayerList noinc(DP_LayerList *ll);

    LayerList(const LayerList &other);
    LayerList(LayerList &&other);

    LayerList &operator=(const LayerList &other);
    LayerList &operator=(LayerList &&other);

    ~LayerList();

    bool isNull() const;

    int count() const;

private:
    explicit LayerList(DP_LayerList *ll);

    DP_LayerList *m_data;
};

}

#endif
