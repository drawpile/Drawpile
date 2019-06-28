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

#ifndef PAINTCORE_CONCURRENT_H
#define PAINTCORE_CONCURRENT_H

#include <QThreadPool>
#include <QSemaphore>
#include <functional>

namespace paintcore {

template<typename T>
void concurrentForEach(QList<T> &list, std::function<void(T)> func)
{
	if(list.isEmpty())
		return;

	if(list.size() == 1) {
		// Just one item: don't bother with threads
		func(list[0]);
		return;
	}

	class ConcurrentForEachRunnable : public QRunnable {
	public:
		T value;
		std::function<void(T)> func;
		QSemaphore *semaphore;

		void run() override
		{
			func(value);
			semaphore->release();
		}
	};

	QSemaphore s;
	QThreadPool *tp = QThreadPool::globalInstance();

	// Run functions in the thread pool
	ConcurrentForEachRunnable *runnables = new ConcurrentForEachRunnable[list.size()];
	for(int i=0;i<list.size();++i) {
		runnables[i].setAutoDelete(false);
		runnables[i].value = list[i];
		runnables[i].func = func;
		runnables[i].semaphore = &s;
		tp->start(&runnables[i]);
	}

	// Wait for all functions to finish
	s.acquire(list.size());
	delete [] runnables;
}

}

#endif

