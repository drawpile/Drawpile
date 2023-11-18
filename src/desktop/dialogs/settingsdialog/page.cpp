// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/page.h"
#include "desktop/utils/widgetutils.h"
#include <QEvent>
#include <QScrollBar>
#include <QVBoxLayout>

namespace dialogs {
namespace settingsdialog {

Page::Page(QWidget *parent)
	: QScrollArea(parent)
{
}

void Page::init(desktop::settings::Settings &settings, bool stretch)
{
	setFrameStyle(QFrame::NoFrame);
	utils::initKineticScrolling(this);

	QWidget *scroll = new QWidget;
	scroll->setContentsMargins(0, 0, 0, 0);
	setWidget(scroll);
	setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setAlignment(Qt::AlignTop);
	scroll->setLayout(layout);

	setUp(settings, layout);

	if(stretch) {
		layout->addStretch(1);
	}
}

}
}
