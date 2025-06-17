// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_NOSCROLL_H
#define DESKTOP_WIDGETS_NOSCROLL_H
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QComboBox>
#include <QWheelEvent>

// Widgets that don't react to wheel events so that you can put them inside a
// QScrollArea without the user accidentally changing them while scrolling.

namespace widgets {

class NoScrollWidget : public QWidget {
public:
	explicit NoScrollWidget(QWidget *parent = nullptr)
		: QWidget(parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};


class NoScrollComboBox : public QComboBox {
public:
	explicit NoScrollComboBox(QWidget *parent = nullptr)
		: QComboBox(parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};


class NoScrollKisSliderSpinBox : public KisSliderSpinBox {
public:
	explicit NoScrollKisSliderSpinBox(QWidget *parent = nullptr)
		: KisSliderSpinBox(parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

}

#endif
