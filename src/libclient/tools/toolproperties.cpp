// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/tools/toolproperties.h"

namespace tools {

std::pair<const QString &, const QVariantHash &> ToolProperties::save() const
{
	return { m_type, m_props };
}

ToolProperties ToolProperties::load(const QString &toolType, const QVariantHash &props)
{
	ToolProperties tp(toolType);
	tp.m_props = props;
	return tp;
}

}
