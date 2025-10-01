// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ANIMATIONPROPERTIESDIALOG_H
#define DESKTOP_DIALOGS_ANIMATIONPROPERTIESDIALOG_H
#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;

namespace dialogs {

class AnimationPropertiesDialog : public QDialog {
	Q_OBJECT
public:
	AnimationPropertiesDialog(
		double framerate, int frameRangeFirst, int frameRangeLast,
		bool compatibilityMode, QWidget *parent = nullptr);

signals:
	void propertiesChanged(
		double framerate, int frameRangeFirst, int frameRangeLast);

private:
	void updateFrameRangeFirst(int frameRangeFirst);
	void updateFrameRangeLast(int frameRangeLast);

	void emitPropertiesChanged();

	QDoubleSpinBox *m_framerateBox;
	QSpinBox *m_frameRangeFirstBox;
	QSpinBox *m_frameRangeLastBox;
};

}

#endif
