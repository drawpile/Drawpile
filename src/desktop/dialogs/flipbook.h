// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FLIPBOOK_H
#define FLIPBOOK_H

#include <QDialog>
#include <QList>
#include <QPixmap>

class Ui_Flipbook;

class QTimer;

namespace canvas {
	class PaintEngine;
}

namespace dialogs {

class Flipbook final : public QDialog
{
	Q_OBJECT
public:
	explicit Flipbook(QWidget *parent=nullptr);
	~Flipbook() override;

	void setPaintEngine(canvas::PaintEngine *pe);

private slots:
	void loadFrame();
	void playPause();
	void rewind();
	void updateFps(int newFps);
	void updateRange();
	void setCrop(const QRectF &rect);
	void resetCrop();

private:
	void resetFrameCache();

	Ui_Flipbook *m_ui;

	canvas::PaintEngine *m_paintengine;
	QList<QPixmap> m_frames;
	QTimer *m_timer;
	QRect m_crop;
	int m_realFps;
};

}

#endif // FLIPBOOK_H
