// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ACTIONPICKERDIALOG_H
#define DESKTOP_DIALOGS_ACTIONPICKERDIALOG_H
#include <QDialog>

class CustomShortcutModel;
class QDialogButtonBox;
class QLineEdit;
class QListView;
class QSortFilterProxyModel;

namespace dialogs {

class ActionPickerDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ActionPickerDialog(QWidget *parent = nullptr);

	void setSelectedAction(const QString &name);

	void accept() override;

Q_SIGNALS:
	void actionSelected(const QString &name);

private:
	void updateButtons();

	CustomShortcutModel *m_customShortcutModel;
	QSortFilterProxyModel *m_proxyModel;
	QLineEdit *m_filterEdit;
	QListView *m_listView;
	QDialogButtonBox *m_buttons;
};

}

#endif
