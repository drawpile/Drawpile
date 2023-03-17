/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2021 Calle Laakkonen

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
#ifndef CANVASSAVERRUNNABLE_H
#define CANVASSAVERRUNNABLE_H

extern "C" {
#include <dpengine/save.h>
}

#include <QObject>
#include <QRunnable>

namespace canvas { class PaintEngine; }

/**
 * @brief A runnable for saving a canvas in a background thread
 *
 * When constructed, a copy of the layerstack is made.
 */
class CanvasSaverRunnable final : public QObject, public QRunnable
{
	Q_OBJECT
public:
	CanvasSaverRunnable(const canvas::PaintEngine *pe, const QString &filename, QObject *parent = nullptr);

	void run() override;

	static QString saveResultToErrorString(DP_SaveResult result);

signals:
	/**
	 * @brief Emitted once the file has been saved
	 * @param error the error message (blank string if no error occurred)
	 */
	void saveComplete(const QString &error);

private:
	const canvas::PaintEngine *m_pe;
	QString m_filename;
};

#endif
