/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

