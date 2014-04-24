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
#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <QScopedPointer>
#include <QSettings>

class QCommandLineParser;
class QCommandLineOption;

class ConfigFile {
public:
	/**
	 * @brief Construct the configuration file reader
	 *
	 * If no filename is given, the object acts as if the settings
	 * file is blank.
	 *
	 * @param filename
	 */
	explicit ConfigFile(const QString &filename);

	/**
	 * @brief Get a value from the configuration file
	 * @param key
	 * @param def
	 * @return
	 */
	QVariant value(const QString &key, const QVariant &def=QVariant()) const;

	/**
	 * @brief Override a value with a command line option
	 *
	 * This function returns the value of the command line option, or if not set,
	 * the equivalent value from the configuration file, or the default if not set there either.
	 *
	 * @param parser
	 * @param option
	 * @return
	 */
	QVariant override(const QCommandLineParser &parser, const QCommandLineOption &option) const;


private:
	QScopedPointer<QSettings> _cfg;
};

#endif
