/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "toolproperties.h"

#include <QSettings>

namespace tools {

void ToolProperties::setValue(const QString &key, const QVariant &value)
{
	m_props[key] = value;
}

QVariant ToolProperties::value(const QString &key, const QVariant &defaultValue) const
{
	return m_props.value(key, defaultValue);
}

int ToolProperties::intValue(const QString &key, int defaultValue, int min, int max) const
{
	bool ok;
	int v = m_props.value(key, defaultValue).toInt(&ok);
	if(!ok)
		v = defaultValue;
	return qBound(min, v, max);
}

bool ToolProperties::boolValue(const QString &key, bool defaultValue) const
{
	return m_props.value(key, defaultValue).toBool();
}

void ToolProperties::save(QSettings &cfg) const
{
	Q_ASSERT(!cfg.group().isEmpty());

	// Remove any old values that may interfere
	cfg.remove(QString());

	QHashIterator<QString, QVariant> i(m_props);
	while(i.hasNext()) {
		i.next();
		cfg.setValue(i.key(), i.value());
	}
	cfg.setValue("tooltype", m_type);
}

ToolProperties ToolProperties::load(const QSettings &cfg)
{
	ToolProperties tp(cfg.value("tooltype").toString());
	for(const QString &key : cfg.allKeys()) {
		tp.m_props[key] = cfg.value(key);
	}
	return tp;
}

}
