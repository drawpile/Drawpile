// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/recover.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/project/recoverymodel.h"
#include "libclient/utils/scopedoverridecursor.h"
#include "libshared/util/paths.h"
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {


RecoveryEntryWidget::RecoveryEntryWidget(
	const project::RecoveryEntry &entry, QWidget *parent)
	: QFrame(parent)
	, m_entry(entry)
{
	setFrameShape(QFrame::Box);
	setFrameShadow(QFrame::Raised);

	QHBoxLayout *rootLayout = new QHBoxLayout;
	setLayout(rootLayout);

	QFrame *thumbnailFrame = new QFrame;
	thumbnailFrame->setContentsMargins(0, 0, 0, 0);
	thumbnailFrame->setFrameShape(QFrame::StyledPanel);
	thumbnailFrame->setFrameShadow(QFrame::Sunken);
	rootLayout->addWidget(thumbnailFrame);

	QVBoxLayout *thumbnailLayout = new QVBoxLayout;
	thumbnailLayout->setContentsMargins(0, 0, 0, 0);
	thumbnailFrame->setLayout(thumbnailLayout);

	QLabel *thumbnailLabel = new QLabel;
	thumbnailLabel->setFixedSize(200, 200);
	thumbnailLabel->setAlignment(Qt::AlignCenter);
	thumbnailLabel->setPixmap(entry.thumbnail());
	thumbnailLayout->addWidget(thumbnailLabel);

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
	case project::RecoveryStatus::Available:
		statusLabel->setText(tr("Available for recovery"));
		break;
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
	recoverButton->setText(tr("Recover"));
	recoverButton->setEnabled(canRecover);
	infoLayout->addWidget(recoverButton);
	connect(
		recoverButton, &QPushButton::clicked, this,
		&RecoveryEntryWidget::requestRecovery);

	QPushButton *removeButton = new QPushButton;
	removeButton->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
	removeButton->setText(tr("Delete"));
	removeButton->setEnabled(canRemove);
	infoLayout->addWidget(removeButton);
	connect(
		removeButton, &QPushButton::clicked, this,
		&RecoveryEntryWidget::requestRemoval);

	infoLayout->addStretch();
}

void RecoveryEntryWidget::requestRecovery()
{
	Q_EMIT recoveryRequested(m_entry.path());
}

void RecoveryEntryWidget::requestRemoval()
{
	Q_EMIT removalRequested(m_entry.path());
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

			if(i != entryCount - 1) {
				utils::addFormSpacer(m_contentLayout);
			}
		}
	}

	m_contentLayout->addStretch();
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

}
}
