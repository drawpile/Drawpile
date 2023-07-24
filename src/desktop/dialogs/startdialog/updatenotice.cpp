// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/updatenotice.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/utils/widgetutils.h"
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

UpdateNotice::UpdateNotice(QWidget *parent)
	: QWidget{parent}
{
	hide();

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

	iconLayout->addWidget(utils::makeIconLabel(
		QIcon::fromTheme("update-none"), QStyle::PM_LargeIconSize, this));

	iconLayout->addSpacing(qMax(0, left - hspace));

	QVBoxLayout *bodyLayout = new QVBoxLayout;
	bodyLayout->setContentsMargins(0, 0, 0, 0);
	bodyLayout->setSpacing(vspace);
	iconLayout->addLayout(bodyLayout);

	m_label = new QLabel;
	m_label->setTextFormat(Qt::RichText);
	m_label->setWordWrap(true);
	utils::setWidgetRetainSizeWhenHidden(m_label, true);
	bodyLayout->addWidget(m_label);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(hspace);
	bodyLayout->addLayout(buttonLayout);

	m_updateButton = new QPushButton;
	m_updateButton->setText(tr("Update"));
	m_updateButton->setIcon(QIcon::fromTheme("update-none"));
	m_updateButton->setCursor(Qt::PointingHandCursor);
	buttonLayout->addWidget(m_updateButton);
	connect(
		m_updateButton, &QAbstractButton::clicked, this,
		&UpdateNotice::openUpdateUrl);

	QPushButton *dismissButton = new QPushButton;
	dismissButton->setText(tr("Dismiss"));
	dismissButton->setToolTip(tr("Hide this notification"));
	buttonLayout->addWidget(dismissButton);
	connect(dismissButton, &QAbstractButton::clicked, this, &QWidget::hide);

	buttonLayout->addStretch();

	layout->addWidget(utils::makeSeparator());
}

void UpdateNotice::setUpdate(const utils::News::Update *update)
{
	m_update = update;
	if(update && update->isValid()) {
		m_label->setText(QStringLiteral("<p style=\"font-size:large;\">%1</p>")
							 .arg(tr("A new version of Drawpile is available!")
									  .toHtmlEscaped()));
		m_updateButton->setToolTip(tr("Update to Drawpile version %1")
									   .arg(update->version.toString()));
		show();
	} else {
		hide();
	}
}

void UpdateNotice::openUpdateUrl()
{
	if(m_update && m_update->isValid()) {
		QString message;
		if(QDesktopServices::openUrl(m_update->url)) {
			message = tr(
				"The download page for Drawpile %1 should have opened in your "
				"browser. If not, please visit <em>drawpile.net</em> "
				"manually.");
		} else {
			message =
				tr("The download page for Drawpile %1 could not be opened, "
				   "please visit <em>drawpile.net</em> manually.");
		}
		m_label->setText(QStringLiteral("<p>%1</p>")
							 .arg(message.arg(m_update->version.toString())));
	}
}

}
}
