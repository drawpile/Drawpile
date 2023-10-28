// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IMPORT_ANIMATIONIMPORTER
#define LIBCLIENT_IMPORT_ANIMATIONIMPORTER
extern "C" {
#include <dpengine/layer_props.h>
#include <dpengine/track.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QObject>
#include <QRunnable>

namespace impex {

class AnimationImporter final : public QObject, public QRunnable {
	Q_OBJECT
public:
	AnimationImporter(const QString &path, int holdTime, int framerate);

	void run() override;

signals:
	void
	finished(const drawdance::CanvasState &canvasState, const QString &error);

private:
	const QString m_path;
	const int m_holdTime;
	const int m_framerate;

	static void setGroupTitle(DP_TransientLayerProps *tlp, int i);
	static void setTrackTitle(DP_TransientTrack *tt, int i);
	static QByteArray getTitle(int i);
};

}

#endif
