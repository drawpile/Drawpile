// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_DOCUMENTMETADATA_H
#define LIBCLIENT_CANVAS_DOCUMENTMETADATA_H
#include <QObject>

namespace drawdance {
class DocumentMetadata;
}

namespace canvas {

class PaintEngine;

class DocumentMetadata final : public QObject {
	Q_OBJECT
public:
	explicit DocumentMetadata(PaintEngine *engine, QObject *parent = nullptr);

	double framerate() const { return m_framerate; }
	int frameCount() const { return m_frameCount; }
	int frameRangeFirst() const { return m_frameRangeFirst; }
	int frameRangeLast() const { return m_frameRangeLast; }

signals:
	void framerateChanged(double fps);
	void frameCountChanged(int frameCount);
	void frameRangeChanged(int frameRangeFirst, int frameRangeLast);

private slots:
	void refreshMetadata(const drawdance::DocumentMetadata &dm);

private:
	PaintEngine *m_engine;
	double m_framerate = 0.0;
	int m_frameCount = 0;
	int m_frameRangeFirst = -1;
	int m_frameRangeLast = -1;
};

}

#endif
