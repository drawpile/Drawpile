// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/projectdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/spinner.h"
#include "libclient/project/projectwrangler.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QLoggingCategory>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <limits>

Q_LOGGING_CATEGORY(
	lcDpProjectDialog, "net.drawpile.dialogs.projectdialog", QtDebugMsg)

namespace dialogs {

ProjectDialog::ProjectDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Project"));
	utils::makeModal(this);
	resize(400, 600);

	QVBoxLayout *layout = new QVBoxLayout(this);

	m_stack = new QStackedWidget;
	layout->addWidget(m_stack, 1);

	m_loadPage = new QWidget;
	m_loadPage->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *loadPageLayout = new QVBoxLayout(m_loadPage);
	loadPageLayout->setContentsMargins(0, 0, 0, 0);

	loadPageLayout->addStretch();

	m_loadLabel = new QLabel(tr("Loading…"));
	m_loadLabel->setAlignment(Qt::AlignCenter);
	m_loadLabel->setTextFormat(Qt::PlainText);
	loadPageLayout->addWidget(m_loadLabel);

	widgets::Spinner *loadPageSpinner = new widgets::Spinner;
	loadPageSpinner->setMaximumHeight(64);
	loadPageLayout->addWidget(loadPageSpinner, 1);

	loadPageLayout->addStretch();

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this,
		&ProjectDialog::requestCancel);
}

void ProjectDialog::openProject(const QString &path)
{
	m_stack->setCurrentWidget(m_loadPage);
	m_buttons->setStandardButtons(QDialogButtonBox::Cancel);

	if(!m_projectWrangler) {
		m_projectWrangler = new project::ProjectWrangler(this);
	}
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

unsigned int ProjectDialog::grabSyncId()
{
	// Extremely unlikely, but just to be totally correct.
	if(m_lastSyncId == std::numeric_limits<unsigned int>::max()) {
		m_lastSyncId = 0u;
	}
	return ++m_lastSyncId;
}

}
