/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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
#ifndef ANIMATIONSAVERRUNNABLE_H
#define ANIMATIONSAVERRUNNABLE_H

extern "C" {
#include <dpengine/save.h>
}

#include <QObject>
#include <QRunnable>

namespace canvas { class PaintEngine; }

/**
 * @brief A runnable for saving the canvas content as an animation in a background thread
 */
class AnimationSaverRunnable final : public QObject, public QRunnable
{
	Q_OBJECT
public:
	using SaveFn = DP_SaveResult (*)(
		DP_CanvasState *, const char *, DP_SaveAnimationProgressFn, void *);

	AnimationSaverRunnable(const canvas::PaintEngine *pe, SaveFn saveFn, const QString &filename, QObject *parent = nullptr);

	void run() override;

public slots:
	void cancelExport();

signals:
	void progress(int progress);
	void saveComplete(const QString &error);

private:
	const canvas::PaintEngine *m_pe;
	QString m_filename;
	SaveFn m_saveFn;
	bool m_cancelled;

	static bool onProgress(void *user, double progress);
};

#endif
