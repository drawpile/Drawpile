// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/canvas_history.h>
}

#include "desktop/dialogs/sessionundodepthlimitdialog.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>


namespace dialogs {

SessionUndoDepthLimitDialog::SessionUndoDepthLimitDialog(
	int undoDepthLimit, QWidget *parent)
	: QDialog{parent}
{
	setModal(true);
	setWindowTitle(tr("Change Session Undo Depth Limit"));
	setLayout(new QVBoxLayout);

	m_undoDepthLimitSpinner = new KisSliderSpinBox{this};
	layout()->addWidget(m_undoDepthLimitSpinner);
	m_undoDepthLimitSpinner->setPrefix(tr("Undo Limit: "));
	m_undoDepthLimitSpinner->setRange(
		DP_CANVAS_HISTORY_UNDO_DEPTH_MIN, DP_CANVAS_HISTORY_UNDO_DEPTH_MAX);
	m_undoDepthLimitSpinner->setValue(undoDepthLimit);

	QLabel *label = new QLabel{this};
	layout()->addWidget(label);
	label->setMinimumWidth(400);
	label->setWordWrap(true);
	label->setText(
		tr("Choose a new undo limit for this session, the current "
		   "limit is %1. Undos are shared between all participants. "
		   "Changing the limit will start the undo stack anew, you won't be "
		   "able to undo before the point where you changed it.")
			.arg(undoDepthLimit));

	QDialogButtonBox *buttons = new QDialogButtonBox{
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
	layout()->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SessionUndoDepthLimitDialog::~SessionUndoDepthLimitDialog() {}

int SessionUndoDepthLimitDialog::undoDepthLimit() const
{
	return m_undoDepthLimitSpinner->value();
}

}
