/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "configfile.h"
#include "../shared/util/logger.h"

#include <QFile>
#if (QT_VERSION < QT_VERSION_CHECK(5, 2, 0))
#include "qcommandlineparser.h"
#else
#include <QCommandLineParser>
#endif

ConfigFile::ConfigFile(const QString &filename)
{
	if(!filename.isEmpty()) {
		if(!QFile(filename).exists()) {
			logger::error() << "Settings file" << filename << "is not readable!";
		} else {
			_cfg.reset(new QSettings(filename, QSettings::IniFormat));
			if(_cfg->status() != QSettings::NoError) {
				_cfg.reset(nullptr);
				logger::error() << "Invalid settings file" << filename;
			}
		}
	}
}

QVariant ConfigFile::value(const QString &key, const QVariant &def) const {
	if(_cfg.isNull())
		return def;
	return _cfg->value(key, def);
}

QVariant ConfigFile::override(const QCommandLineParser &parser, const QCommandLineOption &option) const
{
	const bool isBool = option.valueName().isEmpty();

	if(parser.isSet(option)) {
		if(isBool)
			return true;
		else
			return parser.value(option);

	} else {
		QVariant def;
		if(!option.defaultValues().isEmpty())
			def = option.defaultValues().at(0);
		return value(option.names().at(0), def);
	}
}
