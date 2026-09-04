// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projecteditdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/import/recordingconverter.h"
#include "libclient/io/files.h"
#include "libclient/io/pathinfo.h"
#include "libclient/utils/scopedoverridecursor.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QThreadPool>
#include <QVBoxLayout>
#include <algorithm>
#ifndef Q_OS_ANDROID
#	include <QSaveFile>
#endif

namespace dialogs {

ProjectEditDialog::ProjectEditDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Convert Recordings"));
	utils::makeModal(this);
	resize(400, 300);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_stack = new QStackedWidget;
	dlgLayout->addWidget(m_stack);

	m_inputPage = new QWidget;
	m_inputPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_inputPage);

	QVBoxLayout *inputLayout = new QVBoxLayout(m_inputPage);
	inputLayout->setContentsMargins(0, 0, 0, 0);

	m_inputList = new QListWidget;
	m_inputList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_inputList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_inputList->setDragEnabled(true);
	m_inputList->setAcceptDrops(true);
	m_inputList->setDropIndicatorShown(true);
	m_inputList->setDragDropMode(QAbstractItemView::InternalMove);
	utils::bindKineticScrolling(m_inputList);
	inputLayout->addWidget(m_inputList, 1);
	connect(
		m_inputList, &QListWidget::itemChanged, this,
		&ProjectEditDialog::updateButtons);
	connect(
		m_inputList, &QListWidget::itemSelectionChanged, this,
		&ProjectEditDialog::updateButtons);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(0);
	inputLayout->addLayout(buttonLayout);

	m_addButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_addButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_addButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
	m_addButton->setText(tr("Add"));
	buttonLayout->addWidget(m_addButton);
	connect(
		m_addButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectEditDialog::promptForInputFiles);

	m_removeButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_removeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_removeButton->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
	m_removeButton->setText(tr("Remove"));
	buttonLayout->addWidget(m_removeButton);
	connect(
		m_removeButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectEditDialog::removeSelected);

	buttonLayout->addStretch(1);

	m_moveUpButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_moveUpButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_moveUpButton->setIcon(QIcon::fromTheme("arrow-up"));
	m_moveUpButton->setToolTip(tr("Move up"));
	buttonLayout->addWidget(m_moveUpButton);
	connect(
		m_moveUpButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectEditDialog::moveUpSelected);

	m_moveDownButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_moveDownButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_moveDownButton->setIcon(QIcon::fromTheme("arrow-down"));
	m_moveDownButton->setToolTip(tr("Move down"));
	buttonLayout->addWidget(m_moveDownButton);
	connect(
		m_moveDownButton, &widgets::GroupedToolButton::clicked, this,
		&ProjectEditDialog::moveDownSelected);

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	inputLayout->addWidget(buttons);
	m_saveButton = buttons->button(QDialogButtonBox::Save);
	connect(
		buttons, &QDialogButtonBox::accepted, this, &ProjectEditDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this, &ProjectEditDialog::reject);

	m_progressPage = new QWidget;
	m_progressPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_progressPage);

	QVBoxLayout *progressLayout = new QVBoxLayout(m_progressPage);
	inputLayout->setContentsMargins(0, 0, 0, 0);

	progressLayout->addStretch(1);

	m_progressLabel = new QLabel;
	m_progressLabel->setAlignment(Qt::AlignCenter);
	progressLayout->addWidget(m_progressLabel);

	m_progressBar = new QProgressBar;
	progressLayout->addWidget(m_progressBar);

	QHBoxLayout *progressButtonLayout = new QHBoxLayout;
	progressButtonLayout->setSpacing(0);
	progressLayout->addLayout(progressButtonLayout);

	progressButtonLayout->addStretch(1);

	m_progressCancelButton = new QPushButton(
		QCoreApplication::translate("QPlatformTheme", "Cancel"));
	progressButtonLayout->addWidget(m_progressCancelButton);
	connect(
		m_progressCancelButton, &QPushButton::clicked, this,
		&ProjectEditDialog::cancelRequested);

	progressButtonLayout->addStretch(1);

	progressLayout->addStretch(1);

	m_finishedPage = new QWidget;
	m_finishedPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_finishedPage);

	QVBoxLayout *finishedLayout = new QVBoxLayout(m_finishedPage);
	finishedLayout->setContentsMargins(0, 0, 0, 0);

	finishedLayout->addStretch(1);

	m_finishedLabel = new QLabel(tr("Project file created successfully."));
	m_finishedLabel->setAlignment(Qt::AlignCenter);
	m_finishedLabel->setWordWrap(true);
	finishedLayout->addWidget(m_finishedLabel);

	QHBoxLayout *finishedButtonLayout = new QHBoxLayout;
	finishedButtonLayout->setSpacing(0);
	finishedLayout->addLayout(finishedButtonLayout);

	finishedButtonLayout->addStretch(1);

	m_finishedOpenButton = new QPushButton(
		QIcon::fromTheme(QStringLiteral("document-open")),
		tr("Open project file"));
	finishedButtonLayout->addWidget(m_finishedOpenButton);
	connect(
		m_finishedOpenButton, &QPushButton::clicked, this,
		&ProjectEditDialog::openOutputFile);

	finishedButtonLayout->addStretch(1);
	finishedLayout->addStretch(1);

	QDialogButtonBox *finishedButtons =
		new QDialogButtonBox(QDialogButtonBox::Close);
	finishedLayout->addWidget(finishedButtons);
	connect(
		finishedButtons, &QDialogButtonBox::rejected, this,
		&ProjectEditDialog::reject);

	QPushButton *retryButton =
		finishedButtons->addButton(QDialogButtonBox::Retry);
	retryButton->setText(tr("Back"));
	connect(
		retryButton, &QPushButton::clicked, this,
		&ProjectEditDialog::showInputPage);

	showInputPage();
	updateButtons();
}

void ProjectEditDialog::promptForInputFiles()
{
	addInputPaths(FileWrangler(this).getProjectEditImportPaths());
}

void ProjectEditDialog::addInputPaths(const QStringList &paths)
{
	if(!paths.isEmpty()) {
		QVector<int> indexesToSelect;
		for(const QString &path : paths) {
			handleInputPath(path, indexesToSelect);
		}

		if(!indexesToSelect.isEmpty()) {
			std::sort(indexesToSelect.begin(), indexesToSelect.end());
			m_inputList->setCurrentRow(indexesToSelect.constLast());
			setSelectedIndexes(indexesToSelect);
		}
	}
	updateButtons();
}

void ProjectEditDialog::accept()
{
	int count = m_inputList->count();
	if(count == 0) {
		return;
	}

	m_outputPath = FileWrangler(this).getProjectEditExportPath();
	if(m_outputPath.isEmpty()) {
		return;
	}

	QStringList inputPaths;
	inputPaths.reserve(count);
	for(int i = 0; i < count; ++i) {
		inputPaths.append(m_inputList->item(i)->data(PathRole).toString());
	}

	m_tempFile.reset(new io::TempFile);
	if(!m_tempFile->setTemporaryPath()) {
		onConversionFailed(
			tr("Failed to initialize temporary file."), QString());
		return;
	}

	impex::RecordingConverter *converter =
		new impex::RecordingConverter(inputPaths, m_tempFile, true);
	m_progressLabel->setText(tr("Converting recording(s)…", nullptr, count));
	m_progressBar->setRange(0, 100);
	m_progressBar->setValue(0);

	connect(
		converter, &impex::RecordingConverter::conversionSucceeded, this,
		&ProjectEditDialog::onConversionSucceeded, Qt::QueuedConnection);
	connect(
		converter, &impex::RecordingConverter::conversionCancelled, this,
		&ProjectEditDialog::onConversionCancelled, Qt::QueuedConnection);
	connect(
		converter, &impex::RecordingConverter::conversionFailed, this,
		&ProjectEditDialog::onConversionFailed, Qt::QueuedConnection);
	connect(
		converter, &impex::RecordingConverter::conversionProgress,
		m_progressBar, &QProgressBar::setValue, Qt::QueuedConnection);
	connect(
		this, &ProjectEditDialog::cancelRequested, converter,
		&impex::RecordingConverter::cancel);

	showProgressPage();
	converter->setAutoDelete(true);
	QThreadPool::globalInstance()->start(converter);
}

void ProjectEditDialog::reject()
{
	Q_EMIT cancelRequested();
	QDialog::reject();
}

void ProjectEditDialog::closeEvent(QCloseEvent *event)
{
	Q_EMIT cancelRequested();
	QDialog::closeEvent(event);
}

void ProjectEditDialog::updateButtons()
{
	bool anyItemSelected = false;
	bool topItemSelected = false;
	bool bottomItemSelected = false;

	int count = m_inputList->count();
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_inputList->item(i);
		if(item->isSelected()) {
			anyItemSelected = true;
			if(i == 0) {
				topItemSelected = true;
			}
			if(i == count - 1) {
				bottomItemSelected = true;
			}
		}
	}

	m_moveUpButton->setEnabled(anyItemSelected && !topItemSelected);
	m_moveDownButton->setEnabled(anyItemSelected && !bottomItemSelected);
	m_removeButton->setEnabled(anyItemSelected);
	m_saveButton->setEnabled(count != 0);
}

void ProjectEditDialog::handleInputPath(
	const QString &path, QVector<int> &outIndexesToSelect)
{
	int count = m_inputList->count();
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_inputList->item(i);
		if(item->data(PathRole).toString() == path) {
			outIndexesToSelect.append(i);
			return;
		}
	}

	QListWidgetItem *item = new QListWidgetItem;
	item->setText(io::PathInfo(path).basename());
	item->setToolTip(path);
	item->setData(PathRole, path);
	item->setFlags(
		item->flags() | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
		Qt::ItemNeverHasChildren);
	m_inputList->addItem(item);
	outIndexesToSelect.append(count);
}

void ProjectEditDialog::removeSelected()
{
	int count = m_inputList->count();
	QVector<QListWidgetItem *> itemsToRemove;
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_inputList->item(i);
		if(item->isSelected()) {
			itemsToRemove.append(item);
		}
	}

	for(QListWidgetItem *item : itemsToRemove) {
		delete item;
	}

	updateButtons();
}

void ProjectEditDialog::moveUpSelected()
{
	int currentRow = m_inputList->currentRow();
	int count = m_inputList->count();
	QVector<int> indexesToSelect;
	for(int i = 1; i < count; ++i) {
		QListWidgetItem *item = m_inputList->item(i);
		if(item->isSelected()) {
			m_inputList->takeItem(i);
			m_inputList->insertItem(i - 1, item);
			indexesToSelect.append(i - 1);
		}
	}
	m_inputList->setCurrentRow(qBound(0, currentRow - 1, count - 1));
	setSelectedIndexes(indexesToSelect);
}

void ProjectEditDialog::moveDownSelected()
{
	int currentRow = m_inputList->currentRow();
	int count = m_inputList->count();
	QVector<int> indexesToSelect;
	for(int i = count - 2; i >= 0; --i) {
		QListWidgetItem *item = m_inputList->item(i);
		if(item->isSelected()) {
			m_inputList->takeItem(i);
			m_inputList->insertItem(i + 1, item);
			indexesToSelect.append(i + 1);
		}
	}
	m_inputList->setCurrentRow(qBound(0, currentRow + 1, count - 1));
	setSelectedIndexes(indexesToSelect);
}

void ProjectEditDialog::setSelectedIndexes(const QVector<int> &indexesToSelect)
{
	m_inputList->clearSelection();
	for(int i : indexesToSelect) {
		m_inputList->item(i)->setSelected(true);
	}
}

void ProjectEditDialog::onConversionSucceeded()
{
	QString errorMessage;
	if(copyTemporaryToOutputFile(errorMessage)) {
		finishConversion(true);
	} else {
		onConversionFailed(
			tr("Conversion suceeded, but file saving failed."), errorMessage);
	}
}

void ProjectEditDialog::onConversionCancelled()
{
	finishConversion(false);
}

void ProjectEditDialog::onConversionFailed(
	const QString &message, const QString &detail)
{
	utils::showCritical(this, tr("Error"), message, detail);
	finishConversion(false);
}

bool ProjectEditDialog::copyTemporaryToOutputFile(QString &outErrorMessage)
{
	utils::ScopedOverrideCursor cursor;
	m_progressLabel->setText(tr("Saving project…"));
	m_progressBar->setRange(0, 0);
	QCoreApplication::processEvents();

	if(!m_tempFile) {
		outErrorMessage = tr("No converted file available.");
		return false;
	}

	if(m_outputPath.isEmpty()) {
		outErrorMessage = tr("No file to save to given.");
		return false;
	}

	QFile inputFile(m_tempFile->path());
	if(!inputFile.open(QIODevice::ReadOnly)) {
		outErrorMessage = tr("Error %1 opening input file: %2")
							  .arg(int(inputFile.error()))
							  .arg(inputFile.errorString());
		return false;
	}

#ifdef Q_OS_ANDROID
	QFile outputFile(m_outputPath);
	bool openOk = outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
#else
	QSaveFile outputFile(m_outputPath);
	bool openOk = outputFile.open(QIODevice::WriteOnly);
#endif
	if(!openOk) {
		outErrorMessage = tr("Error %1 opening output file: %2")
							  .arg(int(outputFile.error()))
							  .arg(outputFile.errorString());
		return false;
	}

	if(!io::copyFileContents(inputFile, outputFile, outErrorMessage)) {
		return false;
	}

#ifndef Q_OS_ANDROID
	outputFile.commit();
#endif
	return true;
}

void ProjectEditDialog::finishConversion(bool success)
{
	m_tempFile.clear();
	if(success) {
		showFinishedPage();
	} else {
		showInputPage();
	}
}

void ProjectEditDialog::showInputPage()
{
	m_stack->setCurrentWidget(m_inputPage);
}

void ProjectEditDialog::showProgressPage()
{
	m_stack->setCurrentWidget(m_progressPage);
}

void ProjectEditDialog::showFinishedPage()
{
	m_stack->setCurrentWidget(m_finishedPage);
}

void ProjectEditDialog::openOutputFile()
{
	Q_EMIT openOutputFileRequested(m_outputPath);
	close();
}

}
