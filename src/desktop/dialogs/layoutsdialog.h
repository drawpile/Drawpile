// SPDX-License-Identifier: GPL-3.0-or-later
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
