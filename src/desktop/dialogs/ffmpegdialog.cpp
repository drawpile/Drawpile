// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/ffmpegdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/spinner.h"
#include "libclient/config/config.h"
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

namespace dialogs {

FfmpegDialog::FfmpegDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("FFmpeg Settings"));
	utils::makeModal(this);
	resize(400, 300);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_settingsPage = new QWidget;
	m_settingsPage->setContentsMargins(0, 0, 0, 0);
	m_settingsPage->hide();
	dlgLayout->addWidget(m_settingsPage);

	QVBoxLayout *settingsLayout = new QVBoxLayout(m_settingsPage);
	settingsLayout->setContentsMargins(0, 0, 0, 0);

	settingsLayout->addWidget(new QLabel(tr("FFmpeg path or command:")));

	QHBoxLayout *pathLayout = new QHBoxLayout;
	pathLayout->setContentsMargins(0, 0, 0, 0);
	pathLayout->setSpacing(0);
	settingsLayout->addLayout(pathLayout);

	m_pathEdit = new QLineEdit;
	pathLayout->addWidget(m_pathEdit);

	QPushButton *pathButton = new QPushButton(tr("Choose"));
	pathLayout->addWidget(pathButton);
	connect(
		pathButton, &QPushButton::clicked, this,
		&FfmpegDialog::chooseExecutablePath);

	utils::addFormSeparator(settingsLayout);

	QLabel *explanationLabel = new QLabel;
	explanationLabel->setWordWrap(true);
	explanationLabel->setTextFormat(Qt::RichText);
	explanationLabel->setText(getExplanationLabelText());
	settingsLayout->addWidget(explanationLabel);

	settingsLayout->addStretch();

	QDialogButtonBox *settingsButtons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	settingsLayout->addWidget(settingsButtons);
	connect(
		settingsButtons, &QDialogButtonBox::accepted, this,
		&FfmpegDialog::accept);
	connect(
		settingsButtons, &QDialogButtonBox::rejected, this,
		&FfmpegDialog::reject);

	m_progressPage = new QWidget;
	m_progressPage->setContentsMargins(0, 0, 0, 0);
	m_progressPage->hide();
	dlgLayout->addWidget(m_progressPage);

	QVBoxLayout *progressLayout = new QVBoxLayout(m_progressPage);
	progressLayout->setContentsMargins(0, 0, 0, 0);

	progressLayout->addStretch();

	QLabel *progressLabel = new QLabel(tr("Checking…"));
	progressLabel->setAlignment(Qt::AlignCenter);
	progressLayout->addWidget(progressLabel);

	widgets::Spinner *spinner = new widgets::Spinner;
	spinner->setMaximumHeight(64);
	progressLayout->addWidget(spinner, 1);

	progressLayout->addStretch();

	QDialogButtonBox *progressButtons =
		new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	progressLayout->addWidget(progressButtons);
	connect(
		progressButtons, &QDialogButtonBox::rejected, this,
		&FfmpegDialog::reject);

	m_resultPage = new QWidget;
	m_resultPage->setContentsMargins(0, 0, 0, 0);
	m_resultPage->hide();
	dlgLayout->addWidget(m_resultPage);

	QVBoxLayout *resultLayout = new QVBoxLayout(m_resultPage);
	resultLayout->setContentsMargins(0, 0, 0, 0);

	resultLayout->addStretch();

	m_resultLabel = new QLabel;
	m_resultLabel->setWordWrap(true);
	m_resultLabel->setTextFormat(Qt::RichText);
	m_resultLabel->setAlignment(Qt::AlignCenter);
	resultLayout->addWidget(m_resultLabel);

	m_resultIcon = new QLabel;
	m_resultIcon->setAlignment(Qt::AlignCenter);
	resultLayout->addWidget(m_resultIcon);

	resultLayout->addStretch();

	m_resultButtons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	resultLayout->addWidget(m_resultButtons);
	m_resultButtons->button(QDialogButtonBox::Cancel)->setText(tr("Back"));
	connect(
		m_resultButtons, &QDialogButtonBox::accepted, this,
		&FfmpegDialog::accept);
	connect(
		m_resultButtons, &QDialogButtonBox::rejected, this,
		&FfmpegDialog::reject);

	config::Config *cfg = dpAppConfig();
	QString ffmpegPath = cfg->getFfmpegPath();
#ifndef Q_OS_WINDOWS
	if(ffmpegPath.isEmpty()) {
		ffmpegPath = QStringLiteral("ffmpeg");
	}
#endif
	m_pathEdit->setText(ffmpegPath);

	showPage(Page::Settings);
}

void FfmpegDialog::accept()
{
	switch(m_page) {
	case Page::Settings:
		checkPath(getPath());
		break;
	case Page::Progress:
		break;
	case Page::Result:
		acceptPath(getPath());
		break;
	}
}

void FfmpegDialog::reject()
{
	switch(m_page) {
	case Page::Settings:
		QDialog::reject();
		break;
	case Page::Progress:
		m_cancelled = true;
		if(m_process) {
			m_process->kill();
		}
		break;
	case Page::Result:
		showPage(Page::Settings);
		break;
	}
}

void FfmpegDialog::showPage(Page page)
{
	utils::ScopedUpdateDisabler disabler(this);
	m_page = page;

	m_settingsPage->hide();
	m_progressPage->hide();
	m_resultPage->hide();

	switch(m_page) {
	case Page::Settings:
		m_settingsPage->show();
		break;
	case Page::Progress:
		m_progressPage->show();
		break;
	case Page::Result:
		m_resultPage->show();
		break;
	}
}

void FfmpegDialog::chooseExecutablePath()
{
#ifdef Q_OS_WINDOWS
	QString executableFilter =
		//: Used for picking a kind of file, used like "Executables (*.exe)".
		QStringLiteral("%1 (*.exe)").arg(tr("Executables"));
#else
	QString executableFilter;
#endif
	QString ffmpegPath = QFileDialog::getOpenFileName(
		this, tr("Choose ffmpeg path"), QString(), executableFilter);
	if(!ffmpegPath.isEmpty()) {
		m_pathEdit->setText(ffmpegPath);
	}
}

void FfmpegDialog::checkPath(const QString &ffmpegPath)
{
	if(ffmpegPath.isEmpty()) {
		acceptPath(QString());
	} else {
		showPage(Page::Progress);
		m_cancelled = false;
		m_timeout = false;

		m_process = new QProcess(this);
		m_process->setProgram(ffmpegPath);
		m_process->setArguments({QStringLiteral("-version")});
		m_process->setInputChannelMode(QProcess::ManagedInputChannel);
		m_process->setProcessChannelMode(QProcess::MergedChannels);

		connect(
			m_process, &QProcess::started, this,
			&FfmpegDialog::handleProcessStarted);
		connect(
			m_process,
			QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
			&FfmpegDialog::handleProcessFinished);
		connect(
			m_process, &QProcess::errorOccurred, this,
			&FfmpegDialog::handleProcessErrorOccurred);
		QTimer::singleShot(3000, m_process, [this] {
			if(m_process) {
				m_timeout = true;
				m_process->kill();
			}
		});

		m_process->start();
	}
}

void FfmpegDialog::acceptPath(const QString &ffmpegPath)
{
	dpAppConfig()->setFfmpegPath(ffmpegPath);
	Q_EMIT ffmpegPathAccepted(ffmpegPath);
	QDialog::accept();
}

QString FfmpegDialog::getPath() const
{
	return m_pathEdit->text().trimmed();
}

void FfmpegDialog::handleProcessStarted()
{
	if(m_process) {
		m_process->closeWriteChannel();
	}
}

void FfmpegDialog::handleProcessFinished(
	int exitCode, QProcess::ExitStatus exitStatus)
{
	if(m_process) {
		if(exitStatus == QProcess::NormalExit && exitCode == 0) {
			QRegularExpression re(
				QStringLiteral("ffmpeg\\s*version\\s*(\\S+)"),
				QRegularExpression::CaseInsensitiveOption);
			QRegularExpressionMatch match =
				re.match(QString::fromUtf8(m_process->readAllStandardOutput()));
			if(match.hasMatch()) {
				handleProcessResult(Result::Success, match.captured(1));
			} else {
				handleProcessResult(Result::NotDetected);
			}
		} else if(m_timeout) {
			handleProcessResult(Result::Timeout);
		} else {
			handleProcessResult(Result::Crashed);
		}

		m_process->deleteLater();
		m_process = nullptr;
	}
}

void FfmpegDialog::handleProcessErrorOccurred(QProcess::ProcessError error)
{
	if(m_process && error == QProcess::FailedToStart) {
		handleProcessResult(Result::FailedToStart);
		m_process->deleteLater();
		m_process = nullptr;
	}
}

void FfmpegDialog::handleProcessResult(Result result, const QString &version)
{
	if(m_page == Page::Progress) {
		if(m_cancelled) {
			showPage(Page::Settings);
		} else {
			bool success = false;
			QString icon = QStringLiteral("drawpile_close");
			QString message;
			switch(result) {
			case Result::Success:
				success = true;
				message =
					tr("Successfully detected ffmpeg version %1.").arg(version);
				icon = QStringLiteral("checkbox");
				break;
			case Result::FailedToStart:
				message = tr("Error: the given program could not be started.");
				break;
			case Result::Timeout:
				message = tr("Error: the given program did not finish.");
				break;
			case Result::Crashed:
				message = tr("Error: the given program exited with an error.");
				break;
			case Result::NotDetected:
				message = tr(
					"Error: the given program does not appear to be ffmpeg.");
				break;
			}

			m_resultLabel->setText(message);
			utils::setLabelLargeIconPixmap(
				m_resultIcon, QIcon::fromTheme(icon), m_resultPage);
			m_resultButtons->button(QDialogButtonBox::Ok)->setEnabled(success);
			showPage(Page::Result);
		}
	}
}

QString FfmpegDialog::getExplanationLabelText()
{
#if defined(Q_OS_WINDOWS)
	return QStringLiteral("<p>%1</p><p>%2</p>")
		.arg(
			tr("You can download a Windows version of ffmpeg from %1.")
				.toHtmlEscaped()
				.arg(QStringLiteral(
					"<a href=\"https://ffmpeg.org/download.html"
					"#build-windows\">ffmpeg.org</a>")),
			tr("After downloading and unpacking everything, click on Choose "
			   "and locate the %1 file.")
				.toHtmlEscaped()
				.arg(QStringLiteral("<strong>ffmpeg.exe</strong>")));
#else
	return QStringLiteral("<p>%1</p><p>%2</p>")
		.arg(
#	ifdef Q_OS_MACOS
			tr("You can install ffmpeg through Homebrew.")
#	else
			tr("You can probably install ffmpeg through your package manager.")
#	endif
				.toHtmlEscaped(),
			tr("Once it is installed, just using %1 in the field above should "
			   "work. Otherwise, click on Choose and locate the executable.")
				.toHtmlEscaped()
				.arg(QStringLiteral("<strong>ffmpeg</strong>")));
#endif
}

}
