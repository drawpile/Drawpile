// SPDX-License-Identifier: GPL-3.0-or-later

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
    static constexpr char SEMAPHORE_KEY[] = "drawpile-srv-gui-S";
    static constexpr char MEM_KEY[] = "drawpile-srv-gui-M";

	QSharedMemory m_sharedmem;
	QSystemSemaphore m_semaphore;
};

}
}

#endif
