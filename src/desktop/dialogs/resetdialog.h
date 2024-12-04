// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RESETSESSIONDIALOG_H
#define RESETSESSIONDIALOG_H
#include "libclient/net/message.h"
#include <QDialog>

namespace canvas {
class PaintEngine;
}

namespace drawdance {
class CanvasState;
}

namespace dialogs {

class ResetDialog final : public QDialog {
	Q_OBJECT
public:
	ResetDialog(
		const canvas::PaintEngine *pe, bool compatibilityMode,
		bool singleSession, QWidget *parent = nullptr);

	~ResetDialog() override;

	void setCanReset(bool canReset);

	net::MessageList getResetImage() const;
	QString getResetImageType() const;
	bool isExternalResetImage() const;

signals:
	void resetSelected();
#ifndef SINGLE_MAIN_WINDOW
	void newSelected();
#endif

private slots:
	void onSelectionChanged(int pos);
	void onOpenClicked();

private:
	void onOpenBegin(const QString &fileName);
	void onOpenSuccess(const drawdance::CanvasState &canvasState);
	void onOpenError(const QString &error, const QString &detail);

	struct Private;
	Private *d;
};

}

#endif
