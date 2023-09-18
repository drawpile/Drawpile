// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RESETSESSIONDIALOG_H
#define RESETSESSIONDIALOG_H
#include "libclient/net/message.h"
#include <QDialog>

namespace canvas {
class PaintEngine;
}

namespace dialogs {

class ResetDialog final : public QDialog {
	Q_OBJECT
public:
	ResetDialog(
		const canvas::PaintEngine *pe, bool compatibilityMode,
		QWidget *parent = nullptr);

	~ResetDialog() override;

	void setCanReset(bool canReset);

	net::MessageList getResetImage() const;

signals:
	void resetSelected();
#ifndef SINGLE_MAIN_WINDOW
	void newSelected();
#endif

private slots:
	void onSelectionChanged(int pos);
	void onOpenClicked();

private:
	struct Private;
	Private *d;
};

}

#endif
