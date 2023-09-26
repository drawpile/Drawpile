// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/page.h"
#include "desktop/utils/widgetutils.h"
#include <QEvent>
#include <QScrollBar>
#include <QVBoxLayout>

namespace dialogs {
namespace settingsdialog {

bool Page::eventFilter(QObject *object, QEvent *event)
{
	if(object && object == widget() && event->type() == QEvent::Resize) {
		setMinimumWidth(
			widget()->minimumSizeHint().width() + verticalScrollBar()->width());
	}
	return QScrollArea::eventFilter(object, event);
}

Page::Page(QWidget *parent)
	: QScrollArea(parent)
{
}

void Page::init(desktop::settings::Settings &settings)
{
	setFrameStyle(QFrame::NoFrame);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	utils::initKineticScrolling(this);

	QWidget *scroll = new QWidget;
	scroll->setContentsMargins(0, 0, 0, 0);
	setWidget(scroll);
	setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setAlignment(Qt::AlignTop);
	scroll->setLayout(layout);

	setUp(settings, layout);

	layout->addStretch(1);
}

}
}
