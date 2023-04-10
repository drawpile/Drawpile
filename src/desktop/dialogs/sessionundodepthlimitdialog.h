// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONUNDODEPTHLIMITDIALOG_H
#define SESSIONUNDODEPTHLIMITDIALOG_H
#include <QDialog>

class KisSliderSpinBox;

namespace dialogs {

class SessionUndoDepthLimitDialog final : public QDialog {
	Q_OBJECT
public:
	SessionUndoDepthLimitDialog(int undoDepthLimit, QWidget *parent = nullptr);
	~SessionUndoDepthLimitDialog() override;

	int undoDepthLimit() const;

private:
	KisSliderSpinBox *m_undoDepthLimitSpinner;
};

}

#endif
