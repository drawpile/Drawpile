// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/notifbar.h"
#include "desktop/main.h"
#include <QBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>

namespace widgets {

NotificationBar::NotificationBar(QWidget *parent)
	: QWidget(parent)
{
	hide();

	auto *layout = new QHBoxLayout;
	layout->setContentsMargins(15, 15, 15, 15);
	setLayout(layout);

	m_label = new QLabel(this);
	m_label->setText("Hello\nThere\nthird line\nfourth line");
	layout->addWidget(m_label, 1);

	m_actionButton = new QPushButton(this);
	layout->addWidget(m_actionButton);
	connect(
		m_actionButton, &QPushButton::clicked, this,
		&NotificationBar::actionButtonClicked);
	connect(
		m_actionButton, &QPushButton::clicked, this,
		&NotificationBar::cancelAutoDismissTimer);

	m_closeButton = new QPushButton(this);
	m_closeButton->setIcon(QIcon::fromTheme("cards-block"));
	layout->addWidget(m_closeButton);
	connect(
		m_closeButton, &QPushButton::clicked, this,
		&NotificationBar::closeButtonClicked);
	connect(
		m_closeButton, &QPushButton::clicked, this,
		&NotificationBar::cancelAutoDismissTimer);
	updateCloseButtonText();

	setCursor(Qt::ArrowCursor);
}

void NotificationBar::show(
	const QString &text, const QIcon &actionButtonIcon,
	const QString &actionButtonLabel, RoleColor color, bool allowDismissal)
{
	Q_ASSERT(parentWidget());
	cancelAutoDismissTimer();

	if(color == RoleColor::Warning) {
		setColor(0xed1515, QStringLiteral("#ffffff"));
	} else {
		QPalette pal = dpApp().palette();
		setColor(pal.color(QPalette::Base), QString());
	}

	m_label->setText(text);
	m_actionButton->setIcon(actionButtonIcon);
	m_actionButton->setText(actionButtonLabel);
	m_closeButton->setEnabled(allowDismissal);
	m_closeButton->setVisible(allowDismissal);

	QWidget::show();
}

void NotificationBar::setActionButtonEnabled(bool enabled)
{
	m_actionButton->setEnabled(enabled);
}

bool NotificationBar::isActionButtonEnabled() const
{
	return m_actionButton->isEnabled();
}

void NotificationBar::startAutoDismissTimer()
{
	if(!m_dismissTimer) {
		m_dismissTimer = new QTimer(this);
		m_dismissTimer->setInterval(1000);
		connect(
			m_dismissTimer, &QTimer::timeout, this,
			&NotificationBar::tickAutoDismiss);
	}
	if(!m_dismissTimer->isActive()) {
		m_dismissSeconds = NotificationBar::AUTO_DISMISS_SECONDS;
		m_dismissTimer->start();
		updateCloseButtonText();
	}
}

void NotificationBar::cancelAutoDismissTimer()
{
	if(m_dismissTimer) {
		m_dismissTimer->stop();
		updateCloseButtonText();
	}
}

void NotificationBar::tickAutoDismiss()
{
	if(--m_dismissSeconds > 0) {
		updateCloseButtonText();
	} else {
		m_dismissTimer->stop();
		m_closeButton->click();
	}
}

void NotificationBar::updateCloseButtonText()
{
	m_closeButton->setText(
		m_dismissTimer && m_dismissTimer->isActive()
			//: Countdown to dismissal: "Dismiss (10)", "Dismiss (9)" etc.
			? tr("Dismiss (%1)").arg(m_dismissSeconds)
			: tr("Dismiss"));
}

void NotificationBar::setColor(const QColor &color, const QString &textColor)
{
	m_color = color;
	m_label->setStyleSheet(
		textColor.isEmpty()
			? QString()
			: QStringLiteral("QLabel { color: %1; }").arg(textColor));
	update();
}

void NotificationBar::showEvent(QShowEvent *)
{
	Q_ASSERT(parentWidget());
	updateSize(parentWidget()->size());
	parent()->installEventFilter(this);
}

void NotificationBar::hideEvent(QHideEvent *)
{
	Q_ASSERT(parent());
	parent()->removeEventFilter(this);
	emit heightChanged(0);
}

bool NotificationBar::eventFilter(QObject *obj, QEvent *event)
{
	Q_ASSERT(obj == parent());
	if(event->type() == QEvent::Resize) {
		const QResizeEvent *re = static_cast<const QResizeEvent *>(event);
		updateSize(re->size());
	}
	return false;
}

void NotificationBar::paintEvent(QPaintEvent *e)
{
	QPainter p(this);
	p.fillRect(e->rect(), m_color);
}

void NotificationBar::updateSize(const QSize &parentSize)
{
	int height =
		qMax(
			m_label->sizeHint().height(), m_actionButton->sizeHint().height()) +
		30;
	move(0, 0);
	resize(parentSize.width(), height);
	emit heightChanged(height);
}

}
