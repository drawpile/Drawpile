// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_FFMPEGDIALOG_H
#define DESKTOP_DIALOGS_FFMPEGDIALOG_H
#include <QDialog>
#include <QProcess>

class QDialogButtonBox;
class QLabel;
class QLineEdit;

namespace dialogs {

class FfmpegDialog : public QDialog {
	Q_OBJECT
public:
	explicit FfmpegDialog(QWidget *parent = nullptr);

public Q_SLOTS:
	void accept() override;
	void reject() override;

Q_SIGNALS:
	void ffmpegPathAccepted(const QString &ffmpegPath);

private:
	enum class Page { Settings, Progress, Result };
	enum class Result { Success, FailedToStart, Timeout, Crashed, NotDetected };

	void showPage(Page page);

	void chooseExecutablePath();

	void checkPath(const QString &ffmpegPath);
	void acceptPath(const QString &ffmpegPath);

	QString getPath() const;

	void handleProcessStarted();
	void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void handleProcessErrorOccurred(QProcess::ProcessError error);
	void handleProcessResult(Result result, const QString &version = QString());

	static QString getExplanationLabelText();

	QWidget *m_settingsPage;
	QWidget *m_progressPage;
	QWidget *m_resultPage;
	QLineEdit *m_pathEdit;
	QLabel *m_resultLabel;
	QLabel *m_resultIcon = nullptr;
	QDialogButtonBox *m_resultButtons;
	QProcess *m_process = nullptr;
	Page m_page = Page::Settings;
	bool m_cancelled = false;
	bool m_timeout = false;
};

}

#endif
