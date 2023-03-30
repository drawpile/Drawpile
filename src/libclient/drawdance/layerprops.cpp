extern "C" {
#include <dpengine/layer_props.h>
}

#include "libclient/drawdance/layerprops.h"
#include "libclient/drawdance/layerpropslist.h"
#include "libshared/util/qtcompat.h"

namespace drawdance {

LayerProps LayerProps::inc(DP_LayerProps *lp)
{
    return LayerProps{DP_layer_props_incref_nullable(lp)};
}

LayerProps LayerProps::noinc(DP_LayerProps *lp)
{
    return LayerProps{lp};
}

LayerProps::LayerProps(const LayerProps &other)
    : LayerProps{DP_layer_props_incref_nullable(other.m_data)}
{
}

LayerProps::LayerProps(LayerProps &&other)
    : LayerProps{other.m_data}
{
    other.m_data = nullptr;
}

LayerProps &LayerProps::operator=(const LayerProps &other)
{
    DP_layer_props_decref_nullable(m_data);
    m_data = DP_layer_props_incref_nullable(other.m_data);
    return *this;
}

LayerProps &LayerProps::operator=(LayerProps &&other)
{
    DP_layer_props_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

LayerProps::~LayerProps()
{
    DP_layer_props_decref_nullable(m_data);
}

DP_LayerProps *LayerProps::get() const
{
    return m_data;
}

bool LayerProps::isNull() const
{
    return !m_data;
}

int LayerProps::id() const
{
    return DP_layer_props_id(m_data);
}

QString LayerProps::title() const
{
    size_t length;
    const char *title = DP_layer_props_title(m_data, &length);
    return QString::fromUtf8(title, compat::castSize(length));
}

uint16_t LayerProps::opacity() const
{
    return DP_layer_props_opacity(m_data);
}

int LayerProps::blendMode() const
{
    return DP_layer_props_blend_mode(m_data);
}

bool LayerProps::hidden() const
{
    return DP_layer_props_hidden(m_data);
}

bool LayerProps::censored() const
{
    return DP_layer_props_censored(m_data);
}

bool LayerProps::isolated() const
{
    return DP_layer_props_isolated(m_data);
}

bool LayerProps::isGroup(drawdance::LayerPropsList *outChildren)
{
    DP_LayerPropsList *children = DP_layer_props_children_noinc(m_data);
    if(children) {
        if(outChildren) {
            *outChildren = drawdance::LayerPropsList::inc(children);
        }
        return true;
    } else {
        return false;
    }
}

LayerProps::LayerProps(DP_LayerProps *lp)
    : m_data{lp}
{
}

}
