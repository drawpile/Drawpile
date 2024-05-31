#ifndef DESKTOP_UTILS_CONNECTIONS_H
#define DESKTOP_UTILS_CONNECTIONS_H
#include <QMetaObject>
#include <QObject>

namespace utils {

class Connections final : public QObject {
	Q_OBJECT
public:
	Connections(const QString &key, QObject *parent = nullptr)
		: QObject(parent)
	{
		setObjectName(key);
	}

	void add(const QMetaObject::Connection &con) { m_values.append(con); }

	void clear()
	{
		for(const QMetaObject::Connection &con : m_values) {
			disconnect(con);
		}
	}

private:
	QVector<QMetaObject::Connection> m_values;
};

}

#endif
