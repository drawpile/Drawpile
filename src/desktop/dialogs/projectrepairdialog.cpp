// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectrepairdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/io/files.h"
#include "libclient/io/pathinfo.h"
#include "libclient/project/projectrepair.h"
#include "libclient/utils/scopedoverridecursor.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QStackedWidget>
#include <QThreadPool>
#include <QVBoxLayout>
#include <limits>

namespace dialogs {

ProjectRepairDialog::ProjectRepairDialog(const QString &path, QWidget *parent)
	: QDialog(parent)
	, m_path(path)
{
	setWindowTitle(tr("Repair Project"));
	utils::makeModal(this);
	resize(500, 300);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_stack = new QStackedWidget;
	dlgLayout->addWidget(m_stack);

	m_progressPage = new QWidget;
	m_progressPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_progressPage);

	QVBoxLayout *progressLayout = new QVBoxLayout(m_progressPage);
	progressLayout->setContentsMargins(0, 0, 0, 0);

	progressLayout->addStretch();

	m_progressLabel = new QLabel(tr("Checking…"));
	m_progressLabel->setTextFormat(Qt::PlainText);
	m_progressLabel->setAlignment(Qt::AlignCenter);
	progressLayout->addWidget(m_progressLabel);

	m_progressBar = new QProgressBar;
	m_progressBar->setRange(0, 0);
	m_progressBar->setTextVisible(false);
	progressLayout->addWidget(m_progressBar);

	utils::addFormSpacer(progressLayout);

	QHBoxLayout *progressButtonLayout = new QHBoxLayout;
	progressButtonLayout->setContentsMargins(0, 0, 0, 0);
	progressButtonLayout->addStretch();
	progressLayout->addLayout(progressButtonLayout);

	QPushButton *progressCancelButton = new QPushButton(
		QCoreApplication::translate("QPlatformTheme", "Cancel"));
	progressButtonLayout->addWidget(progressCancelButton);

	progressButtonLayout->addStretch();
	progressLayout->addStretch();

	m_finishedPage = new QWidget;
	m_finishedPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_finishedPage);

	QVBoxLayout *finishedLayout = new QVBoxLayout(m_finishedPage);
	finishedLayout->setContentsMargins(0, 0, 0, 0);

	finishedLayout->addStretch();

	m_finishedLabel = new QLabel;
	m_finishedLabel->setWordWrap(true);
	m_finishedLabel->setTextFormat(Qt::RichText);
	finishedLayout->addWidget(m_finishedLabel);

	utils::addFormSpacer(finishedLayout);

	QHBoxLayout *finishedButtonLayout = new QHBoxLayout;
	finishedButtonLayout->setContentsMargins(0, 0, 0, 0);
	finishedButtonLayout->addStretch();
	finishedLayout->addLayout(finishedButtonLayout);

	m_finishedSaveButton = new QPushButton;
	finishedButtonLayout->addWidget(m_finishedSaveButton);
	connect(
		m_finishedSaveButton, &QPushButton::clicked, this,
		&ProjectRepairDialog::requestSave, Qt::DirectConnection);

	m_finishedCloseButton = new QPushButton;
	finishedButtonLayout->addWidget(m_finishedCloseButton);
	connect(
		m_finishedCloseButton, &QPushButton::clicked, this,
		&ProjectRepairDialog::close, Qt::DirectConnection);

	finishedButtonLayout->addStretch();
	finishedLayout->addStretch();

	m_stack->setCurrentWidget(m_progressPage);

	project::ProjectRepair *pr = new project::ProjectRepair(path);
	connect(
		progressCancelButton, &QPushButton::clicked, pr,
		&project::ProjectRepair::cancel, Qt::DirectConnection);
	connect(
		this, &ProjectRepairDialog::destroyed, pr,
		&project::ProjectRepair::cancel, Qt::DirectConnection);
	connect(
		pr, &project::ProjectRepair::progress, this,
		&ProjectRepairDialog::handleProgress, Qt::QueuedConnection);
	connect(
		pr, &project::ProjectRepair::finished, pr,
		[pr, dlg = QPointer<ProjectRepairDialog>(this)]() {
			if(dlg) {
				dlg->handleFinished(pr);
			}
			pr->deleteLater();
		},
		Qt::QueuedConnection);
	pr->setAutoDelete(false);
	QThreadPool::globalInstance()->start(pr);
}

void ProjectRepairDialog::handleProgress(qint64 inputSize, qint64 repairedSize)
{
	if(m_progressStage == ProgressStage::Initial) {
		m_progressLabel->setText(tr("Repairing…"));
		m_progressStage = ProgressStage::Progressing;
	}

	if(m_progressStage == ProgressStage::Progressing) {
		int max = int(qMin(inputSize, qint64(std::numeric_limits<int>::max())));
		int current =
			int(qMin(repairedSize, qint64(std::numeric_limits<int>::max())));
		if(current < max) {
			m_progressBar->setRange(0, max);
			m_progressBar->setValue(current);
		} else {
			m_progressBar->setRange(0, 0);
			m_progressStage = ProgressStage::Final;
		}
	}
}

void ProjectRepairDialog::handleFinished(project::ProjectRepair *pr)
{
	project::ProjectRepair::Status status = pr->status();
	switch(status) {
	case project::ProjectRepair::Status::Ok:
		if(pr->fileType() == project::ProjectRepair::FileType::Canvas) {
			m_ext = QStringLiteral(".dpcs");
		} else {
			m_ext = QStringLiteral(".dppr");
		}
		m_repairedFile.setSharedPointer(pr->repairedFile().sharedPointer());
		showSave(pr->verified());
		break;
	case project::ProjectRepair::Status::Cancelled:
		showError(tr("Cancelled.").toHtmlEscaped());
		close();
		break;
	default:
		showError(QStringLiteral("<strong>%1</strong> %2")
					  .arg(
						  tr("Error %1:").arg(int(status)).toHtmlEscaped(),
						  pr->errorMessage().toHtmlEscaped()));
		break;
	}
}

void ProjectRepairDialog::showSave(bool verified)
{
	QString finishedText = QStringLiteral("<p>");
	if(verified) {
		finishedText.append(
			tr("The project has been repaired, but no corruption was detected. "
			   "You can choose to save it file anyway, but it may now contain "
			   "invalid data.")
				.toHtmlEscaped());
	} else {
		finishedText.append(
			tr("The project has been repaired, choose a file to save it to.")
				.toHtmlEscaped());
	}
	finishedText.append(QStringLiteral("</p><p>%1</p>")
							.arg(tr("It is strongly recommended that you save "
									"to a new file. Overwriting an existing "
									"file may render it irrecoverable.")
									 .toHtmlEscaped()));
	m_finishedLabel->setText(finishedText);
	m_finishedSaveButton->setText(
		QCoreApplication::translate("QPlatformTheme", "Save"));
	m_finishedCloseButton->setText(
		QCoreApplication::translate("QPlatformTheme", "Cancel"));
	m_stack->setCurrentWidget(m_finishedPage);
}

void ProjectRepairDialog::showError(const QString &message)
{
	m_finishedLabel->setText(message);
	m_finishedSaveButton->hide();
	m_finishedCloseButton->setText(
		QCoreApplication::translate("QPlatformTheme", "Close"));
	m_stack->setCurrentWidget(m_finishedPage);
}

void ProjectRepairDialog::requestSave()
{
	QString suggestedName =
		QStringLiteral("%1-fixed%2%3")
			.arg(
				io::PathInfo(m_path).basenameWithoutExtension(),
				QDateTime::currentDateTime().toString(
					QStringLiteral("yyyyMMddhhmmss")),
				m_ext);

	QString path = FileWrangler(this).getRepairExportPath(suggestedName, m_ext);
	if(!path.isEmpty()) {
		QString error;
		if(saveTo(path, error)) {
			QMessageBox *box = utils::showQuestion(
				this, tr("Saved"),
				tr("The repaired project has been saved, do you want to open "
				   "it now?"));
			connect(
				box, &QMessageBox::accepted, this,
				[this, path] {
					Q_EMIT openRequested(path);
					close();
				},
				Qt::DirectConnection);
			connect(
				box, &QMessageBox::rejected, this, &ProjectRepairDialog::close,
				Qt::DirectConnection);
		} else {
			utils::showCritical(
				this, tr("Error"), tr("Error saving repaired project."), error);
		}
	}
}

bool ProjectRepairDialog::saveTo(const QString &path, QString &outError)
{
	utils::ScopedOverrideCursor overrideCursor;
	return io::copySaveFile(m_repairedFile.path(), path, outError);
}

}
