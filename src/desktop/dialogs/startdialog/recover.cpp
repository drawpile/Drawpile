// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/recover.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/thumbnail.h"
#include "libclient/project/recoverymodel.h"
#include "libclient/utils/scopedoverridecursor.h"
#include "libshared/util/paths.h"
#include <QCheckBox>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <cstring>
#ifdef __EMSCRIPTEN__
#	include "libshared/util/paths.h"
#endif

namespace dialogs {
namespace startdialog {

RecoveryEntryWidget::RecoveryEntryWidget(
	const project::RecoveryEntry &entry, QWidget *parent)
	: QFrame(parent)
	, m_path(entry.path())
	, m_locked(entry.status() == project::RecoveryStatus::Locked)
{
	setFrameShape(QFrame::Box);
	setFrameShadow(QFrame::Raised);

	QHBoxLayout *rootLayout = new QHBoxLayout;
	setLayout(rootLayout);

	widgets::Thumbnail *thumbnail = new widgets::Thumbnail(entry.thumbnail());
	thumbnail->setFixedSize(200, 200);
	thumbnail->setContentsMargins(0, 0, 0, 0);
	thumbnail->setFrameShape(QFrame::StyledPanel);
	thumbnail->setFrameShadow(QFrame::Sunken);
	rootLayout->addWidget(thumbnail);

	QVBoxLayout *infoLayout = new QVBoxLayout;
	rootLayout->addLayout(infoLayout, 1);

	infoLayout->addStretch();

	QLabel *nameLabel = new QLabel;
	nameLabel->setWordWrap(true);
	nameLabel->setTextFormat(Qt::RichText);
	infoLayout->addWidget(nameLabel);

	nameLabel->setText(
		QStringLiteral("<span style=\"font-size:large;\">%1</span>")
			.arg(entry.baseName()));
	nameLabel->setToolTip(entry.path());

	QLabel *mtimeLabel = new QLabel;
	mtimeLabel->setWordWrap(true);
	mtimeLabel->setTextFormat(Qt::RichText);
	infoLayout->addWidget(mtimeLabel);

	QLocale locale;
	QDateTime mtime = entry.mtime().toLocalTime();
	if(mtime.isValid()) {
		//: %1 is a date and time saying when the file was last modified.
		mtimeLabel->setText(
			tr("Modified %1")
				.arg(locale.toString(mtime, QLocale::ShortFormat)));
	} else {
		mtimeLabel->setText(tr("Unknown modification time"));
	}

	QLabel *sizeLabel = new QLabel;
	sizeLabel->setWordWrap(true);
	sizeLabel->setTextFormat(Qt::RichText);
	infoLayout->addWidget(sizeLabel);

	sizeLabel->setText(
		// %1 is a file size, such as "1.23 MB".
		tr("Size: %1").arg(utils::paths::formatFileSize(entry.fileSize())));

	QLabel *statusLabel = new QLabel;
	statusLabel->setWordWrap(true);
	statusLabel->setTextFormat(Qt::RichText);
	infoLayout->addWidget(statusLabel);

	bool canRecover = true;
	bool canRemove = true;
	switch(entry.status()) {
	case project::RecoveryStatus::Available: {
		//: How long you've worked on an autosave file. %1 is either a time span
		//: like "1 hour and 15 minutes" or "unknown".
		statusLabel->setText(
			tr("Work time: %1")
				.arg(utils::formatWorkMinutes(entry.ownWorkMinutes())));
		break;
	}
	case project::RecoveryStatus::Locked:
		statusLabel->setText(tr("Locked by another process"));
		canRemove = false;
		break;
	case project::RecoveryStatus::Error:
		statusLabel->setText(tr("Error: %1").arg(entry.errorMessage()));
		break;
	default:
		statusLabel->setText(tr("Unknown status"));
		break;
	}

	QPushButton *recoverButton = new QPushButton;
	recoverButton->setIcon(QIcon::fromTheme(QStringLiteral("backup")));
#ifdef __EMSCRIPTEN__
	recoverButton->setText(tr("Download"));
#else
	recoverButton->setText(tr("Recover"));
#endif
	recoverButton->setEnabled(canRecover);
	infoLayout->addWidget(recoverButton);
	connect(
		recoverButton, &QPushButton::clicked, this,
#ifdef __EMSCRIPTEN__
		&RecoveryEntryWidget::download
#else
		&RecoveryEntryWidget::save
#endif
	);

	QPushButton *removeButton = new QPushButton;
	removeButton->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
	removeButton->setText(tr("Delete"));
	removeButton->setEnabled(canRemove);
	infoLayout->addWidget(removeButton);
	connect(
		removeButton, &QPushButton::clicked, this,
		&RecoveryEntryWidget::promptRemoval);

	infoLayout->addStretch();
}

void RecoveryEntryWidget::promptRemoval()
{
	QMessageBox *box = utils::makeQuestion(
		this, tr("Delete Autosave"),
		tr("Are you sure you want to permanently delete this autosave file?"),
		tr("Any unrecovered data will be lost."));
	box->button(QMessageBox::Yes)->setText(tr("Yes, delete"));
	box->button(QMessageBox::No)->setText(tr("No, keep"));
	connect(
		box, &QMessageBox::accepted, this,
		&RecoveryEntryWidget::requestRemoval);
	box->show();
}

void RecoveryEntryWidget::requestRemoval()
{
	Q_EMIT removalRequested(m_path);
}

#ifdef __EMSCRIPTEN__
void RecoveryEntryWidget::download()
{
	QByteArray bytes;
	QString error;
	if(utils::paths::slurp(m_path, bytes, error)) {
		FileWrangler(this).saveFileContent(getSuggestedExportBaseName(), bytes);
		utils::showInformation(
			this, tr("Download Started"),
			tr("Your browser should have prompted you to download the file. "
			   "Make sure to check it for completeness afterwards."));
	} else {
		utils::showCritical(
			this, tr("Download Error"),
			tr("Failed to read autosave file: %1").arg(error));
	}
}
#else
void RecoveryEntryWidget::save()
{
	QString savePath = FileWrangler(this).getAutosaveExportPath(
		QString(), getSuggestedExportBaseName());
	if(savePath.isEmpty()) {
		return;
	}

	QString error;
	if(!saveTo(savePath, error)) {
		utils::showCritical(this, tr("Export Error"), error);
		return;
	}

	if(!compareSaved(savePath, error)) {
		utils::showWarning(
			this, tr("Verification Error"), error,
			tr("The file may not have been saved correctly."));
		return;
	}

	QMessageBox *box = utils::makeQuestion(
		this, tr("Exported"),
		tr("Export successful. Do you want to open the file now?"));

	QCheckBox *removeCheckBox = new QCheckBox(tr("Delete original autosave"));
	removeCheckBox->setChecked(!m_locked);
	removeCheckBox->setEnabled(!m_locked);
	box->setCheckBox(removeCheckBox);

	auto triggerRemoval = [this, box] {
		if(!m_locked) {
			QCheckBox *checkBox = box->checkBox();
			if(checkBox && checkBox->isChecked()) {
				Q_EMIT requestRemoval();
			}
		}
	};
	connect(
		box, &QMessageBox::accepted, this, [this, savePath, triggerRemoval] {
			triggerRemoval();
			Q_EMIT openRequested(savePath);
		});
	connect(box, &QMessageBox::rejected, this, triggerRemoval);

	box->show();
}

bool RecoveryEntryWidget::saveTo(const QString &savePath, QString &outError)
{
	// We don't use QFile::copy here because that doesn't work on Android.
	utils::ScopedOverrideCursor overrideCursor;

	QFile inputFile(m_path);
	if(!inputFile.open(QIODevice::ReadOnly)) {
		outError =
			tr("Failed to open autosave file: %1").arg(inputFile.errorString());
		return false;
	}

	QSaveFile saveFile(savePath);
	saveFile.setDirectWriteFallback(true);
	if(!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		outError =
			tr("Failed to open target file: %1").arg(saveFile.errorString());
		return false;
	}

	QByteArray buffer;
	buffer.resize(BUFSIZ);
	while(true) {
		qint64 read = inputFile.read(buffer.data(), BUFSIZ);
		if(read < 0) {
			outError = tr("Failed to read from autosave file: %1")
						   .arg(inputFile.errorString());
			return false;
		} else if(read == 0) {
			break;
		} else {
			qint64 written = saveFile.write(buffer.constData(), read);
			if(written < 0) {
				outError = tr("Failed to write to target file: %1")
							   .arg(saveFile.errorString());
				return false;
			} else if(written != read) {
				outError =
					tr("Failed to write to target file: read/write mismatch");
				return false;
			}
		}
	}

	if(!saveFile.commit()) {
		outError =
			tr("Failed to commit target file: %1").arg(saveFile.errorString());
		return false;
	}

	return true;
}

bool RecoveryEntryWidget::compareSaved(
	const QString &savePath, QString &outError)
{
	QFile inputFile(m_path);
	if(!inputFile.open(QIODevice::ReadOnly)) {
		outError = tr("Failed to open autosave file for verification: %1")
					   .arg(inputFile.errorString());
		return false;
	}

	QFile outputFile(savePath);
	if(!outputFile.open(QIODevice::ReadOnly)) {
		outError = tr("Failed to open target file for verification: %1")
					   .arg(outputFile.errorString());
		return false;
	}

	if(inputFile.size() != outputFile.size()) {
		outError = tr("Verification failed: file sizes do not match");
		return false;
	}

	QByteArray buffer1;
	QByteArray buffer2;
	buffer1.resize(BUFSIZ);
	buffer2.resize(BUFSIZ);
	while(true) {
		qint64 read1 = inputFile.read(buffer1.data(), BUFSIZ);
		if(read1 == -1) {
			outError = tr("Autosave file read error during verification: %1")
						   .arg(inputFile.errorString());
			return false;
		}

		qint64 read2 = outputFile.read(buffer2.data(), BUFSIZ);
		if(read2 == -1) {
			outError = tr("Target file read error during verification: %1")
						   .arg(outputFile.errorString());
			return false;
		}

		if(read1 != read2) {
			outError = tr("Verification failed: read size mismatch");
			return false;
		} else if(read1 == 0) {
			break;
		} else if(
			std::memcmp(buffer1.constData(), buffer2.constData(), read1) != 0) {
			outError =
				tr("Verification failed: autosave and target file data does "
				   "not match");
			return false;
		}
	}

	return true;
}
#endif

QString RecoveryEntryWidget::getSuggestedExportBaseName() const
{
	QString baseName = QFileInfo(m_path).baseName();

	QString ext = QStringLiteral(".dppr");
	if(!baseName.endsWith(ext, Qt::CaseInsensitive)) {
		baseName += ext;
	}

	return QStringLiteral("%1_%2").arg(
		QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddhhmmss")),
		baseName);
}


Recover::Recover(QWidget *parent)
	: Page(parent)
	, m_recoveryModel(new project::RecoveryModel(
		  utils::paths::writablePath(QStringLiteral("autosave")), this))
{
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	setLayout(layout);

	m_scroll = new QScrollArea;
	m_scroll->setWidgetResizable(true);
	utils::bindKineticScrollingWith(
		m_scroll, Qt::ScrollBarAlwaysOff, Qt::ScrollBarAsNeeded);
	layout->addWidget(m_scroll);

	m_content = new QWidget;
	m_scroll->setWidget(m_content);

	m_contentLayout = new QVBoxLayout;
	m_content->setLayout(m_contentLayout);

	connect(
		m_recoveryModel, &project::RecoveryModel::loaded, this,
		&Recover::updateRecoveryEntries);
}

void Recover::activate()
{
	utils::ScopedOverrideCursor overrideCursor;
	Q_EMIT hideLinks();
	m_recoveryModel->load();
}

bool Recover::checkPotentialRecovery()
{
	return m_recoveryModel->checkPotentialEntries();
}

void Recover::updateRecoveryEntries()
{
	utils::ScopedUpdateDisabler disabler(this);

	int scrollPosition;
	if(QScrollBar *bar = m_scroll->verticalScrollBar(); bar) {
		scrollPosition = bar->value();
	} else {
		scrollPosition = -1;
	}

	clearContent();

	QVector<project::RecoveryEntry> entries = m_recoveryModel->entries();
	int entryCount = int(entries.size());
	if(entryCount == 0) {
		QLabel *label = new QLabel;
		label->setWordWrap(true);
		label->setTextFormat(Qt::RichText);
		label->setText(
			QStringLiteral("<em style=\"font-size:large;\">%1</em>")
				.arg(tr("No autosaves to recover.").toHtmlEscaped()));

		QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect;
		effect->setOpacity(0.8);
		label->setGraphicsEffect(effect);

		m_contentLayout->addWidget(label);
	} else {
		for(int i = 0; i < entryCount; ++i) {
			const project::RecoveryEntry &entry = entries[i];
			RecoveryEntryWidget *widget = new RecoveryEntryWidget(entry);
			m_contentLayout->addWidget(widget);
			connect(
				widget, &RecoveryEntryWidget::removalRequested, this,
				&Recover::removePath);
			connect(
				widget, &RecoveryEntryWidget::openRequested, this,
				&Recover::openPath);

			if(i != entryCount - 1) {
				utils::addFormSpacer(m_contentLayout);
			}
		}
	}

	m_contentLayout->addStretch();

	if(scrollPosition >= 0) {
		if(QScrollBar *bar = m_scroll->verticalScrollBar(); bar) {
			bar->setValue(scrollPosition);
		}
	}
}

void Recover::clearContent()
{
	while(!m_contentLayout->isEmpty()) {
		QLayoutItem *item =
			m_contentLayout->takeAt(m_contentLayout->count() - 1);
		QWidget *widget = item->widget();
		if(widget) {
			widget->deleteLater();
		}
		delete item;
	}
}

void Recover::removePath(const QString &path)
{
	QFileInfo fileInfo = QFileInfo(path);
	QString error;
	if(!tryRemovePath(fileInfo, error)) {
		utils::showWarning(
			this, tr("Autosave Removal Failed"),
			tr("Could not remove autosave file %1.").arg(fileInfo.fileName()),
			error);
	}

	m_recoveryModel->removeOrphanedFiles();
	m_recoveryModel->reload();
}

bool Recover::tryRemovePath(const QFileInfo &fileInfo, QString &error)
{
	if(!fileInfo.exists()) {
		error = tr("File not found.");
		return false;
	}

	{
		project::RecoveryEntry entry(fileInfo);
		if(entry.status() == project::RecoveryStatus::Locked) {
			error = tr("File is locked by another process.");
			return false;
		}
	}

	{
		QFile file(fileInfo.filePath());
		if(!file.remove()) {
			error = tr("Error deleting file: %1.").arg(file.errorString());
			return false;
		}
	}

	return true;
}

}
}
