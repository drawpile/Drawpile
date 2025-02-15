// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_LAYOUTSDIALOG_H
#define DESKTOP_DIALOGS_LAYOUTSDIALOG_H
#include <QDialog>
#include <functional>

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
	void
	promptTitle(Layout *layout, const std::function<void(const QString &)> &fn);

	struct Private;
	Private *d;
};

}

#endif
