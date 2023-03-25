extern "C" {
#include <dpengine/layer_props_list.h>
}

#include "libclient/drawdance/layerpropslist.h"

namespace drawdance {

LayerPropsList LayerPropsList::inc(DP_LayerPropsList *lpl)
{
    return LayerPropsList{DP_layer_props_list_incref_nullable(lpl)};
}

LayerPropsList LayerPropsList::noinc(DP_LayerPropsList *lpl)
{
    return LayerPropsList{lpl};
}


LayerPropsList::LayerPropsList()
    : LayerPropsList{nullptr}
{
}

LayerPropsList::LayerPropsList(const LayerPropsList &other)
    : LayerPropsList{DP_layer_props_list_incref_nullable(other.m_data)}
{
}

LayerPropsList::LayerPropsList(LayerPropsList &&other)
    : LayerPropsList{other.m_data}
{
    other.m_data = nullptr;
}

LayerPropsList &LayerPropsList::operator=(const LayerPropsList &other)
{
    DP_layer_props_list_decref_nullable(m_data);
    m_data = DP_layer_props_list_incref_nullable(other.m_data);
    return *this;
}

LayerPropsList &LayerPropsList::operator=(LayerPropsList &&other)
{
    DP_layer_props_list_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

LayerPropsList::~LayerPropsList()
{
    DP_layer_props_list_decref_nullable(m_data);
}

bool LayerPropsList::isNull() const
{
    return !m_data;
}

int LayerPropsList::count() const
{
    return DP_layer_props_list_count(m_data);
}

LayerProps LayerPropsList::at(int index) const
{
    return LayerProps::inc(DP_layer_props_list_at_noinc(m_data, index));
}

LayerPropsList::LayerPropsList(DP_LayerPropsList *lpl)
    : m_data{lpl}
{
}

}
