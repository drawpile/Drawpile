// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOCUMENTMETADATA_H
#define DOCUMENTMETADATA_H

#include <QObject>

namespace drawdance {
   class DocumentMetadata;
}

namespace canvas {

class PaintEngine;

class DocumentMetadata final : public QObject
{
	Q_OBJECT
public:
	explicit DocumentMetadata(PaintEngine *engine, QObject *parent = nullptr);

	int framerate() const { return m_framerate; }
	bool useTimeline() const { return m_useTimeline; }
	int frameCount() const { return m_frameCount; }

signals:
	void framerateChanged(int fps);
	void useTimelineChanged(bool useTimeline);
	void frameCountChanged(int frameCount);

private slots:
	void refreshMetadata(const drawdance::DocumentMetadata &dm);

private:
	PaintEngine *m_engine;
	int m_framerate;
	bool m_useTimeline;
	int m_frameCount;
};

}

#endif // DOCUMENTMETADATA_H
