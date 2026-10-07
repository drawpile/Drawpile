// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTFILTERINPUT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTFILTERINPUT_H
#include "desktop/utils/qtguicompat.h"
#include <QWidget>

class QCheckBox;
class QLineEdit;

namespace dialogs {
namespace settingsdialog {

class ShortcutFilterInput : public QWidget {
	Q_OBJECT
public:
	ShortcutFilterInput(QWidget *parent = nullptr);

	bool isEmpty() const;

	void checkConflictBox();

Q_SIGNALS:
	void filtered(const QString &text);
	void conflictBoxChecked(bool checked);
	void updateRequested();

private:
	void handleFilterTextChanged();
	void handleConflictBoxStateChanged(compat::CheckBoxState state);
	void requestUpdate();
	void handleUpdate();

	QLineEdit *m_filterEdit;
	QCheckBox *m_conflictBox;
	QString m_filterText;
	bool m_showConflicts = false;
	bool m_updatePending = false;
};

}
}

#endif
