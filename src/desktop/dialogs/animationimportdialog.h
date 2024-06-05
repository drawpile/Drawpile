// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ANIMATIONIMPORTDIALOG
#define DESKTOP_DIALOGS_ANIMATIONIMPORTDIALOG
#include "libclient/drawdance/canvasstate.h"
#include <QCollator>
#include <QDialog>

class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTemporaryFile;
class QSpinBox;

namespace color_widgets {
class ColorPreview;
}

namespace dialogs {

class AnimationImportDialog final : public QDialog {
	Q_OBJECT
public:
	enum class Source { Frames = 0, Layers = 1 };

	explicit AnimationImportDialog(int source, QWidget *parent = nullptr);
	~AnimationImportDialog() override;

	static QString getStartPageArgumentForSource(int source);

signals:
	void canvasStateImported(const drawdance::CanvasState &canvasState);

private slots:
#ifndef __EMSCRIPTEN__
	void showColorPicker();
	void chooseFramesFiles();
	void removeSelectedFrames();
	void updateFrameButtons();
#endif
	void chooseLayersFile();
	void updateHoldTimeSuffix(int value);
	void updateImportButton();
	void buttonClicked(QAbstractButton *button);
	void importFinished(
		const drawdance::CanvasState &canvasState, const QString &error);

private:
#ifndef __EMSCRIPTEN__
	void sortFramesPaths(bool ascending, bool numeric);
	void addFramesPaths(QStringList &paths);
	QStringList getFramesPaths() const;
#endif
	void onOpenLayersFile(const QString &path, QTemporaryFile *tempFile);
	void runImport();

	QTabWidget *m_tabs;
#ifndef __EMSCRIPTEN__
	bool m_ascending = true;
	QCollator m_collator;
	color_widgets::ColorPreview *m_backgroundPreview;
	QListWidget *m_framesPathsList;
	QPushButton *m_addButton;
	QPushButton *m_removeButton;
	QPushButton *m_sortButton;
#endif
	QLineEdit *m_layersPathEdit;
	QPushButton *m_chooseButton;
	QSpinBox *m_holdTime;
	QSpinBox *m_framerate;
	QDialogButtonBox *m_buttons;
	QPushButton *m_importButton;
	QTemporaryFile *m_tempFile = nullptr;
};

}

#endif
