#ifndef DRAWDANCE_LAYER_PROPS_LIST_H
#define DRAWDANCE_LAYER_PROPS_LIST_H

#include "libclient/drawdance/layerprops.h"

struct DP_LayerPropsList;

namespace drawdance {

class LayerPropsList final {
public:
    static LayerPropsList inc(DP_LayerPropsList *lpl);
    static LayerPropsList noinc(DP_LayerPropsList *lpl);

    LayerPropsList();
    LayerPropsList(const LayerPropsList &other);
    LayerPropsList(LayerPropsList &&other);

    LayerPropsList &operator=(const LayerPropsList &other);
    LayerPropsList &operator=(LayerPropsList &&other);

    ~LayerPropsList();

    bool isNull() const;

    int count() const;

    LayerProps at(int index) const;

private:
    explicit LayerPropsList(DP_LayerPropsList *lpl);

    DP_LayerPropsList *m_data;
};

}

#endif
