// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MANDATORYFIELDS_H
#define MANDATORYFIELDS_H

#include <QObject>

class QWidget;

/**
 * A utility class that monitors the widgets on a dialog and
 * automatically disables the ok button when any widget
 * with a "mandatoryfield" property is unset.
 */
class MandatoryFields final : public QObject {
	Q_OBJECT
public:
	MandatoryFields(QWidget *parent, QWidget *okButton);

public slots:
	//! Check each registered widget and update the OK button state
	void update();

private:
	void collectFields(QObject *parent);

	QList<QObject*> m_widgets;
	QWidget *m_okButton;
};

#endif

