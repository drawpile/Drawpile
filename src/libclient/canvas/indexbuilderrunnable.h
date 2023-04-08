// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXBUILDERRUNNABLE_H
#define INDEXBUILDERRUNNABLE_H

#include <QRunnable>
#include <QObject>

namespace canvas {

class PaintEngine;

class IndexBuilderRunnable final : public QObject, public QRunnable
{
	Q_OBJECT
public:
	explicit IndexBuilderRunnable(PaintEngine *pe);

	void run() override;

signals:
	void progress(int percent);
	void indexingComplete(bool success, const QString error);

private:
	PaintEngine *m_paintengine;
};

}

#endif // INDEXBUILDERRUNNABLE_H
