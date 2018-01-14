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
	ToolProperties() { }
	ToolProperties(const QString &type)
		: m_type(type) { }

	struct IntValue {
		QString key;
		int defaultValue;
		int min;
		int max;
	};

	struct BoolValue {
		QString key;
		bool defaultValue;
	};

	struct VariantValue {
		QString key;
		QVariant defaultValue;
	};

	/**
	 * @brief Get the type of the tool
	 *
	 * E.g. Freehand and Line tools are both "brush" type tools,
	 * as they share their settings.
	 */
	QString toolType() const { return m_type; }

	/**
	 * @brief Set a value
	 * @param key
	 * @param value
	 */
	void setValue(const QString &key, const QVariant &value);
	template<typename ValueType> void setValue(const ValueType &key, const QVariant &value) { setValue(key.key, value); }

	/**
	 * @brief Get a value
	 * @param key
	 * @param defaultValue
	 * @return
	 */
	QVariant value(const QString &key, const QVariant &defaultValue=QVariant()) const;
	QVariant value(const VariantValue &key) const { return value(key.key, key.defaultValue); }

	/**
	 * @brief Get an integer value within the given bounds
	 * @param key
	 * @param min
	 * @param max
	 * @return
	 */
	int intValue(const QString &key, int defaultValue, int min=0, int max=9999) const;
	int intValue(const IntValue &key) const { return intValue(key.key, key.defaultValue, key.min, key.max); }

	/**
	 * @brief Get a boolean value
	 * @param key
	 * @param defaultValue
	 * @return
	 */
	bool boolValue(const QString &key, bool defaultValue) const;
	bool boolValue(const BoolValue &key) const { return boolValue(key.key, key.defaultValue); }

	//! Are there any stored properties?
	bool isEmpty() const { return m_props.isEmpty(); }

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

	QVariantHash asVariant() const { return m_props; }

	static ToolProperties fromVariant(const QVariantHash &h, const QString &type=QString())
	{
		ToolProperties tp(type);
		tp.m_props = h;
		return tp;
	}

private:
	QVariantHash m_props;
	QString m_type;
};

}

Q_DECLARE_METATYPE(tools::ToolProperties)

#endif
