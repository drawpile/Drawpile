/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

ToolProperties::ToolProperties()
{
}

void ToolProperties::setValue(const QString &key, const QVariant &value)
{
	_props[key] = value;
}

QVariant ToolProperties::value(const QString &key, const QVariant &defaultValue) const
{
	return _props.value(key, defaultValue);
}

int ToolProperties::intValue(const QString &key, int defaultValue, int min, int max) const
{
	bool ok;
	int v = _props.value(key, defaultValue).toInt(&ok);
	if(!ok)
		v = defaultValue;
	return qBound(min, v, max);
}

bool ToolProperties::boolValue(const QString &key, bool defaultValue) const
{
	return _props.value(key, defaultValue).toBool();
}

void ToolProperties::save(QSettings &cfg) const
{
	Q_ASSERT(!cfg.group().isEmpty());

	QHashIterator<QString, QVariant> i(_props);
	while(i.hasNext()) {
		i.next();
		cfg.setValue(i.key(), i.value());
	}
}

ToolProperties ToolProperties::load(const QSettings &cfg)
{
	ToolProperties tp;

	for(const QString &key : cfg.allKeys()) {
		tp._props[key] = cfg.value(key);
	}
	return tp;
}

void ToolsetProperties::save(QSettings &cfg)
{
	// Save tool settings
	QHashIterator<QString, ToolProperties> i(_tools);
	while(i.hasNext()) {
		i.next();
		cfg.beginGroup(i.key());
		i.value().save(cfg);
		cfg.endGroup();
	}

	// Save common settings
	cfg.setValue("foreground", _foreground.name());
	cfg.setValue("background", _background.name());
	cfg.setValue("tool", _currentTool);
}

ToolsetProperties ToolsetProperties::load(QSettings &cfg)
{
	ToolsetProperties tp;

	// Load tool settings
	for(const QString &group : cfg.childGroups()) {
		cfg.beginGroup(group);
		tp._tools[group] = ToolProperties::load(cfg);
		cfg.endGroup();
	}

	// Load common settings
	tp._foreground = QColor(cfg.value("foreground", "black").toString());
	tp._background = QColor(cfg.value("background", "white").toString());
	tp._currentTool = cfg.value("tool", 0).toInt();

	return tp;
}

}
