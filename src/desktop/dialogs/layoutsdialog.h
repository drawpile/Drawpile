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
#ifndef LAYOUTSDIALOG_H
#define LAYOUTSDIALOG_H

#include <QDialog>

class QByteArray;

namespace dialogs {

class LayoutsDialog final : public QDialog {
	Q_OBJECT
public:
	struct Layout;

	explicit LayoutsDialog(
		const QByteArray &currentState, QWidget *parent = nullptr);

	~LayoutsDialog() override;

signals:
	void applyState(const QByteArray &state);

private slots:
	void save();
	void rename();
	void toggleDeleted();
	void updateButtons();
	void applySelected();
	void onFinish(int result);

private:
	bool promptTitle(Layout *layout, QString &outTitle);

	struct Private;
	Private *d;
};

}

#endif
