extern "C" {
#include <dpengine/layer_list.h>
}

#include "libclient/drawdance/layerlist.h"

namespace drawdance {

LayerList LayerList::inc(DP_LayerList *ll)
{
    return LayerList{DP_layer_list_incref_nullable(ll)};
}

LayerList LayerList::noinc(DP_LayerList *ll)
{
    return LayerList{ll};
}

LayerList::LayerList(const LayerList &other)
    : LayerList{DP_layer_list_incref_nullable(other.m_data)}
{
}

LayerList::LayerList(LayerList &&other)
    : LayerList{other.m_data}
{
    other.m_data = nullptr;
}

LayerList &LayerList::operator=(const LayerList &other)
{
    DP_layer_list_decref_nullable(m_data);
    m_data = DP_layer_list_incref_nullable(other.m_data);
    return *this;
}

LayerList &LayerList::operator=(LayerList &&other)
{
    DP_layer_list_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

LayerList::~LayerList()
{
    DP_layer_list_decref_nullable(m_data);
}

bool LayerList::isNull() const
{
    return !m_data;
}

int LayerList::count() const
{
    return DP_layer_list_count(m_data);
}

LayerList::LayerList(DP_LayerList *ll)
    : m_data{ll}
{
}

}
