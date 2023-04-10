// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANIMATIONSAVERRUNNABLE_H
#define ANIMATIONSAVERRUNNABLE_H

extern "C" {
#include <dpengine/save.h>
}

#include "libclient/drawdance/viewmode.h"
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
		DP_CanvasState *, DP_ViewModeBuffer *, const char *, DP_SaveAnimationProgressFn, void *);

	AnimationSaverRunnable(const canvas::PaintEngine *pe, SaveFn saveFn, const QString &filename, QObject *parent = nullptr);

	void run() override;

public slots:
	void cancelExport();

signals:
	void progress(int progress);
	void saveComplete(const QString &error);

private:
	const canvas::PaintEngine *m_pe;
	drawdance::ViewModeBuffer m_vmb;
	QString m_filename;
	SaveFn m_saveFn;
	bool m_cancelled;

	static bool onProgress(void *user, double progress);
};

#endif
