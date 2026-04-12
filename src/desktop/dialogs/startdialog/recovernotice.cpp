// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/recovernotice.h"
#include "desktop/utils/widgetutils.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

RecoverNotice::RecoverNotice(QWidget *parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	QStyle *s = style();
	int left = s->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this);
	int top = s->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, this);
	int right = s->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this);
	int hspace = s->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
	int vspace = s->pixelMetric(QStyle::PM_LayoutVerticalSpacing);

	QHBoxLayout *iconLayout = new QHBoxLayout;
	iconLayout->setContentsMargins(left, top, right, vspace);
	iconLayout->setSpacing(hspace);
	layout->addLayout(iconLayout);

	iconLayout->addWidget(
		utils::makeIconLabel(
			QIcon::fromTheme(QStringLiteral("drawpile_backup_off")), this));

	iconLayout->addSpacing(qMax(0, left - hspace));

	QVBoxLayout *bodyLayout = new QVBoxLayout;
	bodyLayout->setContentsMargins(0, 0, 0, 0);
	bodyLayout->setSpacing(vspace);
	iconLayout->addLayout(bodyLayout);

	QLabel *label = new QLabel(tr("You have unsaved files to recover!"));
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(true);
	bodyLayout->addWidget(label);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(hspace);
	bodyLayout->addLayout(buttonLayout);

	QPushButton *recoverButton = new QPushButton;
	recoverButton->setIcon(QIcon::fromTheme(QStringLiteral("backup")));
	recoverButton->setText(tr("Recover"));
	buttonLayout->addWidget(recoverButton);
	connect(
		recoverButton, &QPushButton::clicked, this,
		&RecoverNotice::switchToRecoverPageRequested);

	QPushButton *dismissButton = new QPushButton;
	dismissButton->setText(tr("Dismiss"));
	buttonLayout->addWidget(dismissButton);
	connect(dismissButton, &QPushButton::clicked, this, &RecoverNotice::hide);

	buttonLayout->addStretch();

	layout->addWidget(utils::makeSeparator());
}

}
}
