// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SELECTIONALTERDIALOG_H
#define DESKTOP_DIALOGS_SELECTIONALTERDIALOG_H
#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QDialogButtonBox;
class QSpinBox;

namespace dialogs {

class SelectionAlterDialog final : public QDialog {
	Q_OBJECT
public:
	SelectionAlterDialog(QWidget *parent = nullptr);

signals:
	void alterSelectionRequested(int expand, int feather, bool fromEdge);

private:
	static constexpr char PROP_SHRINK[] = "SelectionAlterDialog_shrink";
	static constexpr char PROP_EXPAND[] = "SelectionAlterDialog_expand";
	static constexpr char PROP_FEATHER[] = "SelectionAlterDialog_feather";
	static constexpr char PROP_FROM_EDGE[] = "SelectionAlterDialog_fromEdge";

	static bool getBoolProperty(QWidget *w, const char *name);
	static int getIntProperty(QWidget *w, const char *name);

	void updateControls();
	void emitAlterSelectionRequested();

	QButtonGroup *m_expandGroup;
	QSpinBox *m_expandSpinner;
	QSpinBox *m_featherSpinner;
	QCheckBox *m_fromEdgeBox;
	QDialogButtonBox *m_buttons;
};

}

#endif
