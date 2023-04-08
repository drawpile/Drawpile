// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLPROPERTIES_H
#define TOOLPROPERTIES_H

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
	ToolProperties() = default;
	explicit ToolProperties(const QString &type)
		: m_type(type)
	{
		Q_ASSERT(!type.isEmpty());
	}

	template<typename Type>
	struct Value {
		QString key;
		Type defaultValue;
	};

	template<typename Type>
	struct RangedValue {
		QString key;
		Type defaultValue;
		Type min;
		Type max;
	};

	/**
	 * @brief Get the type of the tool
	 *
	 * E.g. Freehand and Line tools are both "brush" type tools,
	 * as they share their settings.
	 */
	QString toolType() const { return m_type; }

	template<typename ValueType> void setValue(const ValueType &key, const decltype(ValueType::defaultValue) &value)
	{
		Q_ASSERT(!m_type.isEmpty());
		m_props[key.key] = QVariant::fromValue(value);
	}

	template<typename Type> Type value(const Value<Type> &v) const
	{
		QVariant var = m_props.value(v.key);
		if(var.isNull() || !var.canConvert<Type>())
			return v.defaultValue;
		return var.value<Type>();
	}

	template<typename Type> Type value(const RangedValue<Type> &v) const
	{
		QVariant var = m_props.value(v.key);
		if(var.isNull() || !var.canConvert<Type>())
			return v.defaultValue;

		return qBound(
			v.min,
			var.value<Type>(),
			v.max
		);
	}

	QColor value(const Value<QColor> &v) const
	{
		const QColor c = m_props.value(v.key).value<QColor>();
		if(!c.isValid())
			return v.defaultValue;
		return c;
	}

	bool isEmpty() const { return m_props.isEmpty(); }

	/**
	 * @brief Save tool properties
	 *
	 * The properties will be saved under a group
	 * named by toolType()
	 *
	 * @param cfg
	 */
	void save(QSettings &cfg) const;

	/**
	 * @brief Load tool properties
	 * @param cfg
	 * @param toolType
	 *
	 * @return properties
	 */
	static ToolProperties load(QSettings &cfg, const QString &toolType);

private:
	QVariantHash m_props;
	QString m_type;
};

}

Q_DECLARE_METATYPE(tools::ToolProperties)

#endif
