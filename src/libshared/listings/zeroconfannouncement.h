// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ZEROCONFANNOUNCEMENT_H
#define ZEROCONFANNOUNCEMENT_H

#include <QObject>
#include <QMap>

namespace KDNSSD {
	class PublicService;
}

class ZeroConfAnnouncement final : public QObject
{
	Q_OBJECT
public:
	explicit ZeroConfAnnouncement(QObject *parent=nullptr);

	void publish(quint16 port);
	void stop();

public slots:
	void setTitle(const QString &title);

private:
	KDNSSD::PublicService *m_service = nullptr;
	QMap<QString, QByteArray> m_textData;
};

#endif // ZEROCONFANNOUNCEMENT_H
