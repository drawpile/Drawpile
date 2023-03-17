/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
