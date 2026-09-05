// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/banner.h"
#include "desktop/utils/widgetutils.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace widgets {

Banner::Banner(
	const QIcon &icon, const QString &text, Qt::TextFormat textFormat,
	bool dismissable, QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *widgetLayout = new QVBoxLayout(this);
	widgetLayout->setContentsMargins(0, 0, 0, 0);

	QFrame *frame = new QFrame;
	frame->setFrameShape(QFrame::StyledPanel);
	frame->setFrameShadow(QFrame::Sunken);
	widgetLayout->addWidget(frame);

	QHBoxLayout *frameLayout = new QHBoxLayout(frame);

	if(!icon.isNull()) {
		frameLayout->addWidget(utils::makeIconLabel(icon, this));
	}

	QLabel *label = new QLabel(text);
	label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	label->setTextFormat(textFormat);
	label->setWordWrap(true);
	frameLayout->addWidget(label, 1);
	connect(label, &QLabel::linkActivated, this, &Banner::linkActivated);

	if(dismissable) {
		QToolButton *dismissButton = new QToolButton;
		dismissButton->setAutoRaise(true);
		dismissButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		dismissButton->setToolTip(tr("Dismiss"));
		dismissButton->setIcon(
			QIcon::fromTheme(QStringLiteral("drawpile_close")));
		frameLayout->addWidget(dismissButton, 0, Qt::AlignRight | Qt::AlignTop);
		connect(
			dismissButton, &QToolButton::clicked, this, &Banner::deleteLater);
	}

	utils::addFormSpacer(widgetLayout);
}

}
