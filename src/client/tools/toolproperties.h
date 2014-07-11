/*
   Drawpile - a collaborative drawing program.

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
#ifndef TOOLPROPERTIES_H
#define TOOLPROPERTIES_H

#include <QHash>
#include <QVariant>
#include <QColor>

class QSettings;

namespace tools {

/**
 * @brief Settings for a single tool
 */
class ToolProperties
{
public:
	ToolProperties();

	/**
	 * @brief Set a value
	 * @param key
	 * @param value
	 */
	void setValue(const QString &key, const QVariant &value);

	/**
	 * @brief Get a value
	 * @param key
	 * @param defaultValue
	 * @return
	 */
	QVariant value(const QString &key, const QVariant &defaultValue=QVariant()) const;

	/**
	 * @brief Get an integer value within the given bounds
	 * @param key
	 * @param min
	 * @param max
	 * @return
	 */
	int intValue(const QString &key, int defaultValue, int min=0, int max=9999) const;

	/**
	 * @brief Get a boolean value
	 * @param key
	 * @param defaultValue
	 * @return
	 */
	bool boolValue(const QString &key, bool defaultValue) const;

	/**
	 * @brief Save tool properties
	 *
	 * The correct settings group should be set before calling this
	 * @param cfg
	 */
	void save(QSettings &cfg) const;

	/**
	 * @brief Load tool properties
	 *
	 * The correct settings group should be set before calling this
	 * @param cfg
	 * @return properties
	 */
	static ToolProperties load(const QSettings &cfg);

private:
	QHash<QString, QVariant> _props;
};

/**
 * @brief Settings for the whole set of tools
 *
 * This contains:
 * - individual tool settings
 * - the currently selected tool
 * - foreground/background color shared by all tools within this set
 */
class ToolsetProperties {
public:
	void setTool(const QString &toolname, const ToolProperties &props) { _tools[toolname] = props; }

	ToolProperties &tool(const QString &toolname) { return _tools[toolname]; }
	ToolProperties tool(const QString &toolname) const { return _tools[toolname]; }

	void setForegroundColor(const QColor &color) { _foreground = color; }
	void setBackgroundColor(const QColor &color) { _background = color; }

	const QColor &foregroundColor() const { return _foreground; }
	const QColor &backgroundColor() const { return _background; }

	void setCurrentTool(int tool) { _currentTool = tool; }
	int currentTool() const { return _currentTool; }

	/**
	 * @brief Load all tool settings
	 *
	 * Note. The settings file group should be set before calling this
	 * @param cfg
	 * @return
	 */
	static ToolsetProperties load(QSettings &cfg);

	/**
	 * @brief Save all tool settings
	 *
	 * Note. The settings file group should be set before calling this
	 * @param cfg
	 */
	void save(QSettings &cfg);

private:
	QHash<QString, ToolProperties> _tools;
	QColor _foreground;
	QColor _background;
	int _currentTool;
};

}

#endif // TOOLPROPERTIES_H
