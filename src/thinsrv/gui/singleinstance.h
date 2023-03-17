/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QObject>
#include <QSharedMemory>
#include <QSystemSemaphore>

namespace server {
namespace gui {

/**
 * @brief Make sure only a single instance of this application is started
 */
class SingleInstance final : public QObject
{
	Q_OBJECT
public:
	explicit SingleInstance(QObject *parent=nullptr);
	~SingleInstance() override;

	/**
	 * @brief Try acquiring the single instance lock
	 *
	 * If another instance is already running, false is returned.
	 */
	bool tryStart();

signals:
private:
	QSharedMemory m_sharedmem;
	QSystemSemaphore m_semaphore;
};

}
}

#endif
