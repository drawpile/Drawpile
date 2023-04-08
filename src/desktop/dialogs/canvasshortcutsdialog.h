// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CANVASSHORTCUTSDIALOG_H
#define CANVASSHORTCUTSDIALOG_H

#include "libclient/canvas/canvasshortcuts.h"
#include <QDialog>

class CanvasShortcutsModel;

namespace dialogs {

class CanvasShortcutsDialog final : public QDialog {
	Q_OBJECT
public:
	explicit CanvasShortcutsDialog(
		const CanvasShortcuts::Shortcut *s,
		const CanvasShortcutsModel &canvasShortcuts, QWidget *parent);

	~CanvasShortcutsDialog() override;

	CanvasShortcuts::Shortcut shortcut() const;

private:
	void updateType();
	void updateAction();
	void updateResult();

	struct Private;
	Private *d;
};

}

#endif
