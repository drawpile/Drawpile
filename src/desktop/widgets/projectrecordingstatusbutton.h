// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_PROJECTRECORDINGSTATUSBUTTON_H
#define DESKTOP_WIDGETS_PROJECTRECORDINGSTATUSBUTTON_H
#include <QToolButton>

namespace canvas {
class CanvasModel;
}

namespace widgets {

class ProjectRecordingStatusButton : public QToolButton {
	Q_OBJECT
public:
	explicit ProjectRecordingStatusButton(QWidget *parent = nullptr);

	void setCanvas(canvas::CanvasModel *canvas);

protected:
	bool event(QEvent *e) override;

private:
	void onProjectRecordingStarted();
	void onProjectRecordingStopped();
	void setProjectRecordingActive(bool active);

	canvas::CanvasModel *m_canvas = nullptr;
};

}

#endif
