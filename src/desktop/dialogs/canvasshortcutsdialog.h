/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
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
