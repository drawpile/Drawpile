// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IMPORT_ANIMATIONIMPORTER
#define LIBCLIENT_IMPORT_ANIMATIONIMPORTER
extern "C" {
#include <dpengine/layer_props.h>
#include <dpengine/track.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QByteArray>
#include <QObject>
#include <QRunnable>
#include <QStringList>
#include <QVector>

namespace impex {

class AnimationImporter : public QObject, public QRunnable {
	Q_OBJECT
public:
	AnimationImporter(int holdTime, int framerate);

	void run() override;

signals:
	void
	finished(const drawdance::CanvasState &canvasState, const QString &error);

protected:
	virtual DP_CanvasState *load(DP_LoadResult *outResult) = 0;

	const int m_holdTime;
	const int m_framerate;

	static void setLayerTitle(DP_TransientLayerProps *tlp, int i);
	static void setGroupTitle(DP_TransientLayerProps *tlp, int i);
	static void setTrackTitle(DP_TransientTrack *tt, int i);
	static QByteArray getGroupOrTrackTitle(int i);
};


class AnimationLayersImporter final : public AnimationImporter {
	Q_OBJECT
public:
	AnimationLayersImporter(const QString &path, int holdTime, int framerate);

protected:
	DP_CanvasState *load(DP_LoadResult *outResult) override;

private:
	const QString m_path;
};


class AnimationFramesImporter final : public AnimationImporter {
	Q_OBJECT
public:
	AnimationFramesImporter(
		const QStringList &paths, int holdTime, int framerate);

protected:
	DP_CanvasState *load(DP_LoadResult *outResult) override;

private:
	static QVector<QByteArray> pathsToUtf8(const QStringList &paths);
	static const char *getPathAt(void *user, int index);

	const QVector<QByteArray> m_pathsBytes;
};

}

#endif
