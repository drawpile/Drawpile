// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/layer_content.h>
}

#include "libclient/drawdance/image.h"
#include "libclient/drawdance/layercontent.h"

namespace drawdance {

LayerContent LayerContent::null()
{
	return LayerContent{nullptr};
}

LayerContent LayerContent::inc(DP_LayerContent *lc)
{
	return LayerContent{DP_layer_content_incref_nullable(lc)};
}

LayerContent LayerContent::noinc(DP_LayerContent *lc)
{
	return LayerContent{lc};
}

LayerContent::LayerContent(const LayerContent &other)
	: LayerContent{DP_layer_content_incref_nullable(other.m_data)}
{
}

LayerContent::LayerContent(LayerContent &&other)
	: LayerContent{other.m_data}
{
	other.m_data = nullptr;
}

LayerContent &LayerContent::operator=(const LayerContent &other)
{
	DP_layer_content_decref_nullable(m_data);
	m_data = DP_layer_content_incref_nullable(other.m_data);
	return *this;
}

LayerContent &LayerContent::operator=(LayerContent &&other)
{
	DP_layer_content_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

LayerContent::~LayerContent()
{
	DP_layer_content_decref_nullable(m_data);
}

DP_LayerContent *LayerContent::get() const
{
	return m_data;
}

bool LayerContent::isNull() const
{
	return !m_data;
}

QColor LayerContent::sampleColorAt(
	uint16_t *stampBuffer, int x, int y, int diameter, bool opaque,
	bool pigment, int &lastDiameter) const
{
	DP_UPixelFloat pixel = DP_layer_content_sample_color_at(
		m_data, stampBuffer, x, y, diameter, opaque, pigment, &lastDiameter);
	QColor color;
	color.setRgbF(pixel.r, pixel.g, pixel.b, pixel.a);
	return color;
}

QImage LayerContent::toImage(const QRect &rect) const
{
	if(rect.isEmpty()) {
		return QImage{};
	} else {
		DP_Pixel8 *pixels = DP_layer_content_to_pixels8(
			m_data, rect.x(), rect.y(), rect.width(), rect.height());
		return wrapPixels8(rect.width(), rect.height(), pixels);
	}
}

QImage LayerContent::toImageMask(const QRect &rect, const QColor &color) const
{
	if(rect.isEmpty()) {
		return QImage{};
	} else {
		DP_UPixel8 c = {color.rgba()};
		DP_Pixel8 *pixels = DP_layer_content_to_pixels8_mask(
			m_data, rect.x(), rect.y(), rect.width(), rect.height(), c);
		return wrapPixels8(rect.width(), rect.height(), pixels);
	}
}

LayerContent::LayerContent(DP_LayerContent *lc)
	: m_data{lc}
{
}

}
