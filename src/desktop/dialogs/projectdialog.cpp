// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/spinner.h"
#include "desktop/widgets/thumbnail.h"
#include "libclient/project/projectwrangler.h"
#include "libshared/net/protover.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <limits>

Q_LOGGING_CATEGORY(
	lcDpProjectDialog, "net.drawpile.dialogs.projectdialog", QtWarningMsg)

namespace dialogs {

ProjectDialog::ProjectDialog(bool dirty, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Project Statistics"));
	utils::makeModal(this);
	resize(600, 600);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QFrame *dirtyFrame = new QFrame;
	dirtyFrame->setFrameShape(QFrame::StyledPanel);
	dirtyFrame->setFrameShadow(QFrame::Sunken);
	layout->addWidget(dirtyFrame);

	QHBoxLayout *dirtyLayout = new QHBoxLayout(dirtyFrame);

	dirtyLayout->addWidget(
		utils::makeIconLabel(
			QIcon::fromTheme(
				dirty ? QStringLiteral("dialog-warning")
					  : QStringLiteral("dialog-information")),
			dirtyFrame));

	QString dirtyLabelText;
	if(dirty) {
		dirtyLabelText.append(
			QStringLiteral("<strong>%1</strong> ")
				.arg(tr("The canvas has changes not saved in the project!")
						 .toHtmlEscaped()));
	}
	dirtyLabelText.append(
		tr("These statistics only reflect sessions saved in the project. "
		   "Sessions where you disabled autosave or quit without saving and "
		   "otherwise unsaved changes will not be present.")
			.toHtmlEscaped());

	QLabel *dirtyLabel = new QLabel(dirtyLabelText);
	dirtyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	dirtyLabel->setTextFormat(Qt::RichText);
	dirtyLabel->setWordWrap(true);
	dirtyLayout->addWidget(dirtyLabel, 1);

	QToolButton *dirtyCloseButton = new QToolButton;
	dirtyCloseButton->setAutoRaise(true);
	dirtyCloseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	dirtyCloseButton->setToolTip(tr("Dismiss"));
	dirtyCloseButton->setIcon(
		QIcon::fromTheme(QStringLiteral("drawpile_close")));
	dirtyLayout->addWidget(dirtyCloseButton, 0, Qt::AlignRight | Qt::AlignTop);
	connect(
		dirtyCloseButton, &QToolButton::clicked, dirtyFrame,
		&QFrame::deleteLater);

	QSpacerItem *dirtySpacer = utils::addFormSpacer(layout);
	connect(
		dirtyFrame, &QFrame::destroyed, this,
		[layout, dirtySpacer] {
			layout->removeItem(dirtySpacer);
			delete dirtySpacer;
		},
		Qt::QueuedConnection);

	m_stack = new QStackedWidget;
	layout->addWidget(m_stack, 1);

	m_loadPage = makeLoadPage(m_loadLabel);
	m_stack->addWidget(m_loadPage);

	m_errorPage = makeErrorPage(m_errorLabel);
	m_stack->addWidget(m_errorPage);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this,
		&ProjectDialog::requestCancel);

	m_stack->setCurrentWidget(m_loadPage);
}

void ProjectDialog::openProject(const QString &path)
{
	utils::ScopedUpdateDisabler disabler(this);
	m_stack->setCurrentWidget(m_loadPage);
	m_buttons->setStandardButtons(QDialogButtonBox::Cancel);

	if(!m_projectWrangler) {
		m_projectWrangler = new project::ProjectWrangler(this);
		connect(
			m_projectWrangler, &project::ProjectWrangler::openErrorOccurred,
			this, &ProjectDialog::showErrorPage, Qt::QueuedConnection);
		connect(
			m_projectWrangler, &project::ProjectWrangler::overviewErrorOccurred,
			this, &ProjectDialog::showErrorPage, Qt::QueuedConnection);
		connect(
			m_projectWrangler,
			&project::ProjectWrangler::unhandledErrorOccurred, this,
			&ProjectDialog::showUnhandledErrorMessageBox, Qt::QueuedConnection);
		connect(
			m_projectWrangler, &project::ProjectWrangler::syncReceived, this,
			&ProjectDialog::handleSync, Qt::QueuedConnection);
		connect(
			m_projectWrangler, &project::ProjectWrangler::openSucceeded, this,
			&ProjectDialog::handleOpenSucceeded, Qt::QueuedConnection);
		connect(
			m_projectWrangler, &project::ProjectWrangler::overviewGenerated,
			this, &ProjectDialog::handleOverviewGenerated,
			Qt::QueuedConnection);
	}

	m_projectWrangler->openProject(path);
}

void ProjectDialog::requestCancel()
{
	m_stack->setEnabled(false);
	if(m_projectWrangler) {
		m_cancelSyncId = grabSyncId();
		qCDebug(lcDpProjectDialog, "Request cancel %u", m_cancelSyncId);
		m_projectWrangler->requestCancel(m_cancelSyncId);
	} else {
		qCDebug(lcDpProjectDialog, "Cancel now");
		reject();
	}
}

void ProjectDialog::showErrorPage(const QString &errorMessage)
{
	utils::ScopedUpdateDisabler disabler(this);
	m_stack->setCurrentWidget(m_errorPage);
	m_errorLabel->setText(errorMessage);
	m_buttons->setStandardButtons(QDialogButtonBox::Close);
}

void ProjectDialog::showUnhandledErrorMessageBox(const QString &errorMessage)
{
	QString objectName = QStringLiteral("unhandlederrorbox");
	QMessageBox *box =
		findChild<QMessageBox *>(objectName, Qt::FindDirectChildrenOnly);
	if(box) {
		qCWarning(
			lcDpProjectDialog,
			"Unhandled error while another one is being presented: %s",
			qUtf8Printable(errorMessage));
	} else {
		box = utils::makeWarning(this, tr("Unexpected Error"), errorMessage);
		box->setInformativeText(tr("This is probably a bug in Drawpile."));
		box->setObjectName(objectName);
		box->show();
	}
}

void ProjectDialog::handleSync(unsigned int syncId)
{
	if(syncId == 0) {
		qCWarning(lcDpProjectDialog, "Got null sync id");
	} else if(syncId == m_cancelSyncId) {
		qCDebug(lcDpProjectDialog, "Got cancel sync id %u", syncId);
		reject();
	} else {
		qCDebug(
			lcDpProjectDialog, "Got sync id %u, but not waiting for it",
			syncId);
	}
}

void ProjectDialog::handleOpenSucceeded()
{
	utils::ScopedUpdateDisabler disabler(this);
	m_stack->setCurrentWidget(m_loadPage);
	m_buttons->setStandardButtons(QDialogButtonBox::Cancel);
	m_projectWrangler->generateOverview();
}

void ProjectDialog::handleOverviewGenerated()
{
	utils::ScopedUpdateDisabler disabler(this);

	QWidget *overviewPage = new QWidget;
	overviewPage->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *overviewLayout = new QVBoxLayout(overviewPage);
	overviewLayout->setContentsMargins(0, 0, 0, 0);

	m_projectWrangler->withOverviewEntries(
		[&](const QVector<project::OverviewEntry> &overviewEntries) {
			QFormLayout *form = new QFormLayout;
			overviewLayout->addLayout(form);

			long long totalSessionMinutes = 0LL;
			long long totalOwnWorkMinutes = 0LL;
			for(const project::OverviewEntry &oe : overviewEntries) {
				if(oe.openedAt.isValid() && oe.closedAt.isValid()) {
					totalSessionMinutes +=
						qRound(double(oe.openedAt.secsTo(oe.closedAt)) / 60.0);
				}
				if(oe.ownWorkMinutes > 0LL) {
					totalOwnWorkMinutes += oe.ownWorkMinutes;
				}
			}

			QLabel *totalSessionTimeLabel =
				new QLabel(utils::formatWorkMinutes(totalSessionMinutes));
			totalSessionTimeLabel->setWordWrap(true);
			form->addRow(tr("Total session time:"), totalSessionTimeLabel);

			QLabel *totalOwnWorkTimeLabel =
				new QLabel(utils::formatWorkMinutes(totalOwnWorkMinutes));
			totalOwnWorkTimeLabel->setWordWrap(true);
			form->addRow(tr("Your total work time:"), totalOwnWorkTimeLabel);

			utils::addFormSpacer(overviewLayout);

			QScrollArea *scroll = new QScrollArea;
			scroll->setWidgetResizable(true);
			utils::bindKineticScrolling(scroll);
			overviewLayout->addWidget(scroll, 1);

			QWidget *sessionsWidget = new QWidget;
			QVBoxLayout *sessionsLayout = new QVBoxLayout(sessionsWidget);
			scroll->setWidget(sessionsWidget);

			QLocale locale;
			int sessionCount = int(overviewEntries.size());
			for(int i = 0; i < sessionCount; ++i) {
				const project::OverviewEntry &oe = overviewEntries[i];

				QHBoxLayout *sessionLayout = new QHBoxLayout;
				sessionsLayout->addLayout(sessionLayout);
				sessionLayout->setSpacing(12);

				widgets::Thumbnail *thumbnail =
					new widgets::Thumbnail(oe.thumbnail);
				thumbnail->setFixedSize(200, 200);
				thumbnail->setContentsMargins(0, 0, 0, 0);
				thumbnail->setFrameShape(QFrame::StyledPanel);
				thumbnail->setFrameShadow(QFrame::Sunken);
				sessionLayout->addWidget(thumbnail);

				QFormLayout *sessionForm = new QFormLayout;
				sessionLayout->addLayout(sessionForm);

				QLabel *sessionTitleLabel =
					new QLabel(QStringLiteral("<strong>%1</strong>")
								   .arg(tr("Session %1")
											.arg(oe.sessionId)
											.toHtmlEscaped()));
				sessionTitleLabel->setWordWrap(true);
				sessionForm->addRow(sessionTitleLabel);

				QLabel *sessionOpenLabel = new QLabel(oe.openedAt.toString(
					locale.dateTimeFormat(QLocale::ShortFormat)));
				sessionOpenLabel->setWordWrap(true);
				//: Refers to the date and time a session was opened (started.)
				sessionForm->addRow(tr("Opened at:"), sessionOpenLabel);

				QLabel *sessionCloseLabel = new QLabel(oe.closedAt.toString(
					locale.dateTimeFormat(QLocale::ShortFormat)));
				sessionCloseLabel->setWordWrap(true);
				//: Refers to the date and time a session was closed (ended.)
				sessionForm->addRow(tr("Closed at:"), sessionCloseLabel);

				QString sessionTime;
				if(oe.openedAt.isValid() && oe.closedAt.isValid()) {
					sessionTime = utils::formatWorkMinutes(
						qRound(double(oe.openedAt.secsTo(oe.closedAt)) / 60.0));
				} else {
					//: Part of "Session time: unknown".
					sessionTime = tr("unknown");
				}
				QLabel *sessionTimeLabel = new QLabel(sessionTime);
				sessionTimeLabel->setWordWrap(true);
				sessionForm->addRow(tr("Session time:"), sessionTimeLabel);

				QLabel *sessionWorkTimeLabel =
					new QLabel(utils::formatWorkMinutes(oe.ownWorkMinutes));
				sessionWorkTimeLabel->setWordWrap(true);
				sessionForm->addRow(
					tr("Your work time:"), sessionWorkTimeLabel);

				protocol::ProtocolVersion protover =
					protocol::ProtocolVersion::fromString(oe.protocol);
				if(!protover.isCurrent()) {
					QString compatibilityText;
					if(protover.isPastCompatible()) {
						QString versionName = protover.versionName();
						if(versionName.isEmpty()) {
							compatibilityText =
								tr("Recorded with an older, but compatible "
								   "version of Drawpile.");
						} else {
							compatibilityText =
								// %1 is a version number.
								tr("Recorded with an older, but compatible "
								   "version (%1) of Drawpile.")
									.arg(versionName);
						}
					} else if(protover.isFutureMinorIncompatibility()) {
						compatibilityText =
							tr("Recorded with a newer version of Drawpile with "
							   "minor incompatibilities.");
					} else if(protover.isPast()) {
						QString versionName = protover.versionName();
						if(versionName.isEmpty()) {
							compatibilityText =
								tr("Recorded with an old incompatible version "
								   "of Drawpile.");
						} else {
							compatibilityText =
								// %1 is a version number.
								tr("Recorded with an old, incompatible version "
								   "(%1) of Drawpile.")
									.arg(versionName);
						}
					} else if(protover.isFuture()) {
						compatibilityText =
							tr("Recorded with a new, incompatible version of "
							   "Drawpile.");
					} else {
						compatibilityText =
							tr("Recorded with an unknown incompatible version "
							   "of Drawpile.");
					}

					QLabel *sessionCompatibilityLabel =
						new QLabel(compatibilityText);
					sessionCompatibilityLabel->setWordWrap(true);
					sessionForm->addRow(sessionCompatibilityLabel);
				}

				if(i != sessionCount - 1) {
					utils::addFormSeparator(sessionsLayout);
				}
			}

			sessionsLayout->addStretch(1);
		});

	m_stack->addWidget(overviewPage);
	m_stack->setCurrentWidget(overviewPage);

	if(m_overviewPage) {
		m_stack->removeWidget(m_overviewPage);
		delete m_overviewPage;
	}

	m_overviewPage = overviewPage;

	m_buttons->setStandardButtons(QDialogButtonBox::Close);
}

unsigned int ProjectDialog::grabSyncId()
{
	// Extremely unlikely, but just to be totally correct.
	if(m_lastSyncId == std::numeric_limits<unsigned int>::max()) {
		m_lastSyncId = 0u;
	}
	return ++m_lastSyncId;
}

QWidget *ProjectDialog::makeLoadPage(QLabel *&label)
{
	QWidget *page = new QWidget;
	page->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(page);
	layout->setContentsMargins(0, 0, 0, 0);

	layout->addStretch();

	label = new QLabel(tr("Loading…"));
	label->setAlignment(Qt::AlignCenter);
	label->setTextFormat(Qt::PlainText);
	layout->addWidget(label);

	widgets::Spinner *spinner = new widgets::Spinner;
	spinner->setMaximumHeight(64);
	layout->addWidget(spinner, 1);

	layout->addStretch();
	return page;
}

QWidget *ProjectDialog::makeErrorPage(QLabel *&label)
{
	QWidget *page = new QWidget;
	page->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *errorPageLayout = new QVBoxLayout(page);
	errorPageLayout->setContentsMargins(0, 0, 0, 0);

	errorPageLayout->addStretch();

	label = new QLabel;
	label->setAlignment(Qt::AlignCenter);
	label->setTextFormat(Qt::PlainText);
	errorPageLayout->addWidget(label);

	errorPageLayout->addStretch();
	return page;
}

}
