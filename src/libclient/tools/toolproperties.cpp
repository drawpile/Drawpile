// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/tools/toolproperties.h"

#include <QSettings>

namespace tools {

void ToolProperties::save(QSettings &cfg) const
{
	if(m_type.isEmpty())
		return;

	cfg.beginGroup(m_type);

	// Remove any old values that may interfere
	cfg.remove(QString());

	QHashIterator<QString, QVariant> i(m_props);
	while(i.hasNext()) {
		i.next();
		cfg.setValue(i.key(), i.value());
	}

	cfg.endGroup();
}

ToolProperties ToolProperties::load(QSettings &cfg, const QString &toolType)
{
	Q_ASSERT(!toolType.isEmpty());
	ToolProperties tp(toolType);
	cfg.beginGroup(toolType);
	for(const QString &key : cfg.allKeys()) {
		tp.m_props[key] = cfg.value(key);
	}
	cfg.endGroup();
	return tp;
}

}

