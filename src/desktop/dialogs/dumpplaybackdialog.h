// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DUMPPLAYBACKDIALOG_H
#define DUMPPLAYBACKDIALOG_H

#include <QDialog>

namespace canvas {
class CanvasModel;
}

namespace drawdance {
class CanvasHistorySnapshot;
}

namespace dialogs {

class DumpPlaybackDialog final : public QDialog {
	Q_OBJECT
public:
	explicit DumpPlaybackDialog(
		canvas::CanvasModel *canvas, QWidget *parent = nullptr);

	~DumpPlaybackDialog() override;

protected:
	void closeEvent(QCloseEvent *) override;

private slots:
	void onDumpPlaybackAt(
		long long pos, const drawdance::CanvasHistorySnapshot &chs);
	void playPause();
	void singleStep();
	void jumpToPreviousReset();
	void jumpToNextReset();
	void jump();

private:
	void updateUi();
	void updateTables();
	void updateHistoryTable();
	void updateForkTable();
	void updateStatus(
		int historyCount, int historyOffset, int forkCount, int forkStart,
		int forkFallbehind);

	struct Private;
	Private *d;
};

}

#endif
