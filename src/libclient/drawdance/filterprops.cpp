// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/filter_props.h>
}
#include "libclient/drawdance/filterprops.h"

namespace drawdance {

FilterProps FilterProps::makeHsvAdjust(float h, float s, float v)
{
	return noinc(DP_filter_props_new_hsv_adjust(h, s, v));
}

FilterProps FilterProps::makeHslAdjust(float h, float s, float l)
{
	return noinc(DP_filter_props_new_hsl_adjust(h, s, l));
}

FilterProps FilterProps::makeHcyAdjust(float h, float c, float y)
{
	return noinc(DP_filter_props_new_hcy_adjust(h, c, y));
}

FilterProps FilterProps::null()
{
	return FilterProps(nullptr);
}

FilterProps FilterProps::inc(DP_FilterProps *lp)
{
	return FilterProps(DP_filter_props_incref_nullable(lp));
}

FilterProps FilterProps::noinc(DP_FilterProps *lp)
{
	return FilterProps(lp);
}

FilterProps::FilterProps(const FilterProps &other)
	: FilterProps(DP_filter_props_incref_nullable(other.m_data))
{
}

FilterProps::FilterProps(FilterProps &&other)
	: FilterProps(other.m_data)
{
	other.m_data = nullptr;
}

FilterProps &FilterProps::operator=(const FilterProps &other)
{
	DP_filter_props_decref_nullable(m_data);
	m_data = DP_filter_props_incref_nullable(other.m_data);
	return *this;
}

FilterProps &FilterProps::operator=(FilterProps &&other)
{
	DP_filter_props_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

FilterProps::~FilterProps()
{
	DP_filter_props_decref_nullable(m_data);
}

DP_FilterProps *FilterProps::get() const
{
	return m_data;
}

bool FilterProps::isNull() const
{
	return !m_data;
}

QByteArray FilterProps::serialize()
{
	QByteArray buf;
	DP_filter_props_serialize(m_data, &FilterProps::getSerializeBuffer, &buf);
	return buf;
}

FilterProps::FilterProps(DP_FilterProps *lp)
	: m_data(lp)
{
}

unsigned char *FilterProps::getSerializeBuffer(void *user, size_t size)
{
	QByteArray &buf = *static_cast<QByteArray *>(user);
	buf.resize(size);
	return reinterpret_cast<unsigned char *>(buf.data());
}

}
