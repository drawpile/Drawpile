// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_EXPANDSHRINKSPINNER_H
#define DESKTOP_WIDGETS_EXPANDSHRINKSPINNER_H
#include <QWidget>

class QActionGroup;
class QSpinBox;
class QToolButton;

namespace widgets {

class ExpandShrinkSpinner : public QWidget {
	Q_OBJECT
public:
	ExpandShrinkSpinner(bool slider = true, QWidget *parent = nullptr);

	void setBlockUpdateSignalOnDrag(bool block);

	void setSpinnerRange(int min, int max);
	void setSpinnerValue(int value);
	int spinnerValue() const;
	int effectiveValue() const;

	void setShrink(bool shrink);
	bool isShrink() const { return m_shrink; }

	void setKernel(int kernel);
	int kernel() const { return m_kernel; }

signals:
	void spinnerValueChanged(int value);
	void shrinkChanged(bool shrink);
	void kernelChanged(int kernel);

private:
	void updateDirectionLabels();
	void updateKernelLabel();
	static void
	updateButtonLabelFromAction(QToolButton *button, QAction *action);

	QSpinBox *m_spinner;
	QToolButton *m_directionButton;
	QToolButton *m_kernelButton;
	QAction *m_expandAction;
	QAction *m_shrinkAction;
	QAction *m_roundAction;
	QAction *m_squareAction;
	bool m_shrink;
	int m_kernel;
};

}

#endif
