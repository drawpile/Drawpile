// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/resetnoticedialog.h"
#include "desktop/utils/widgetutils.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace dialogs {

ResetNoticeDialog::ResetNoticeDialog(
	const drawdance::CanvasState &canvasState, bool compatibilityMode,
	QWidget *parent)
	: QDialog{parent}
	, m_canvasState{canvasState}
	, m_closeButton{nullptr}
{
	setModal(false);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	m_messageLabel = new QLabel{this};
	m_messageLabel->setWordWrap(true);
	if(compatibilityMode) {
		m_messageLabel->setText(
			tr("The session has been reset. Since this is a Drawpile 2.1 "
			   "session and you're running Drawpile 2.2, this probably "
			   "changed how things on the canvas look. Do you want to save "
			   "the canvas as it was before the reset?"));
	} else {
		m_messageLabel->setText(
			tr("The session has been reset. Normally, everything on the "
			   "canvas should look the same as it did before, but that's "
			   "not guaranteed. Do you want to save the canvas as it was "
			   "before the reset?"));
	}
	layout->addWidget(m_messageLabel);

	m_progressBar = new QProgressBar{this};
	m_progressBar->setRange(0, 100);
	utils::setWidgetRetainSizeWhenHidden(m_progressBar, true);
	m_progressBar->setVisible(false);
	layout->addWidget(m_progressBar);

	QDialogButtonBox *buttons =
		new QDialogButtonBox{QDialogButtonBox::Close, this};
	layout->addWidget(buttons);

	m_saveButton = new QPushButton{
		QIcon::fromTheme("document-save-as"), tr("Save As..."), buttons};
	buttons->addButton(m_saveButton, QDialogButtonBox::ActionRole);

	m_closeButton = buttons->button(QDialogButtonBox::Close);

	connect(
		buttons, &QDialogButtonBox::clicked, this,
		&ResetNoticeDialog::buttonClicked);
}

void ResetNoticeDialog::setSaveInProgress(bool saveInProgress)
{
	m_saveButton->setDisabled(saveInProgress);
}

void ResetNoticeDialog::catchupProgress(int percent)
{
	m_closeButton->setDisabled(percent < 100);
	m_progressBar->setVisible(true);
	m_progressBar->setValue(percent);
	if(percent < 100) {
		m_progressBar->setFormat(tr("Restoring Canvas %p%"));
	} else {
		m_progressBar->setFormat(tr("Canvas Restored"));
	}
}

void ResetNoticeDialog::buttonClicked(QAbstractButton *button)
{
	if(button == m_saveButton) {
		emit saveRequested(m_canvasState);
	} else if(button == m_closeButton) {
		close();
	}
}

}
