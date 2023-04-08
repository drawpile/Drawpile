// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/layer_group.h>
}

#include "libclient/drawdance/image.h"
#include "libclient/drawdance/layergroup.h"
#include "libclient/drawdance/layerprops.h"

namespace drawdance {

LayerGroup LayerGroup::null()
{
    return LayerGroup{nullptr};
}

LayerGroup LayerGroup::inc(DP_LayerGroup *lg)
{
    return LayerGroup{DP_layer_group_incref_nullable(lg)};
}

LayerGroup LayerGroup::noinc(DP_LayerGroup *lg)
{
    return LayerGroup{lg};
}

LayerGroup::LayerGroup(const LayerGroup &other)
    : LayerGroup{DP_layer_group_incref_nullable(other.m_data)}
{
}

LayerGroup::LayerGroup(LayerGroup &&other)
    : LayerGroup{other.m_data}
{
    other.m_data = nullptr;
}

LayerGroup &LayerGroup::operator=(const LayerGroup &other)
{
    DP_layer_group_decref_nullable(m_data);
    m_data = DP_layer_group_incref_nullable(other.m_data);
    return *this;
}

LayerGroup &LayerGroup::operator=(LayerGroup &&other)
{
    DP_layer_group_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

LayerGroup::~LayerGroup()
{
    DP_layer_group_decref_nullable(m_data);
}

bool LayerGroup::isNull() const
{
    return !m_data;
}

QImage LayerGroup::toImage(const LayerProps &layerProps, const QRect &rect) const
{
    if(rect.isEmpty()) {
        return QImage{};
    } else {
        DP_Pixel8 *pixels = DP_layer_group_to_pixels8(
            m_data, layerProps.get(), rect.x(), rect.y(), rect.width(), rect.height());
        return wrapPixels8(rect.width(), rect.height(), pixels);
    }
}

LayerGroup::LayerGroup(DP_LayerGroup *lg)
    : m_data{lg}
{
}

}
