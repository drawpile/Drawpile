// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ANIMATIONIMPORTDIALOG
#define DESKTOP_DIALOGS_ANIMATIONIMPORTDIALOG
#include "libclient/drawdance/canvasstate.h"
#include <QDialog>

class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace dialogs {

class AnimationImportDialog final : public QDialog {
	Q_OBJECT
public:
	explicit AnimationImportDialog(QWidget *parent = nullptr);
	~AnimationImportDialog() override;

signals:
	void canvasStateImported(const drawdance::CanvasState &canvasState);

private slots:
	void chooseFile();
	void updateHoldTimeSuffix(int value);
	void updateImportButton(const QString &path);
	void buttonClicked(QAbstractButton *button);
	void importFinished(
		const drawdance::CanvasState &canvasState, const QString &error);

private:
	QLineEdit *m_pathEdit;
	QPushButton *m_chooseButton;
	QSpinBox *m_holdTime;
	QSpinBox *m_framerate;
	QDialogButtonBox *m_buttons;
	QPushButton *m_importButton;

	void runImport();
};

}

#endif
