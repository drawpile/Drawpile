/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "titlewidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QAbstractButton>
#include <QPainter>
#include <QStyleOptionButton>
#include "../../libshared/qtshims.h"

namespace docks {

/**
 * A window button for the custom title bar.
 *
 * This is a modified version of Qt's internal QDockWidgetTitleButton.
 */
class TitleWidget::Button : public QAbstractButton
{
public:
	Button(const QIcon &icon, QWidget *parent);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override { return sizeHint(); }

	void enterEvent(shim::EnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
};

TitleWidget::Button::Button(const QIcon &icon, QWidget *parent)
	: QAbstractButton(parent)
{
	setIcon(icon);
	setFocusPolicy(Qt::NoFocus);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize TitleWidget::Button::sizeHint() const
{
	ensurePolished();

	const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
	int size = style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, nullptr, this);
	const QSize sz = icon().actualSize(QSize(iconSize, iconSize));
	size += qMax(sz.width(), sz.height());

	return QSize(size, size);
}

void TitleWidget::Button::enterEvent(shim::EnterEvent *event)
{
	if (isEnabled()) update();
	QAbstractButton::enterEvent(event);
}

void TitleWidget::Button::leaveEvent(QEvent *event)
{
	if (isEnabled()) update();
	QAbstractButton::leaveEvent(event);
}

void TitleWidget::Button::paintEvent(QPaintEvent *)
{
	// Make the button invisible when it's not enabled.
	// This way we can hide the button without affecting the layout.
	if(!isEnabled())
		return;

	QPainter p(this);

	icon().paint(
		&p,
		QRect(1, 1, width()-2, height()-2),
		Qt::AlignCenter,
		underMouse() ? QIcon::Active : QIcon::Normal,
		isDown() ? QIcon::On : QIcon::Off
		);
}

TitleWidget::TitleWidget(QDockWidget *parent) : QWidget(parent)
{
	m_layout = new QHBoxLayout;
	m_layout->setSpacing(0);
	m_layout->setContentsMargins(6, 2, 1, 2);
	setLayout(m_layout);

	// (un)dock and close buttons
	m_dockButton = new Button(style()->standardIcon(QStyle::SP_TitleBarNormalButton), this);
	connect(m_dockButton, &QAbstractButton::clicked, parent, [parent]() {
		parent->setFloating(!parent->isFloating());
	});
	m_layout->addWidget(m_dockButton);

	m_closeButton = new Button(style()->standardIcon(QStyle::SP_TitleBarCloseButton), this);
	connect(m_closeButton, &QAbstractButton::clicked, parent, &QDockWidget::close);
	m_layout->addWidget(m_closeButton);

	onFeaturesChanged(parent->features());
	connect(parent, &QDockWidget::featuresChanged, this, &TitleWidget::onFeaturesChanged);
}

void TitleWidget::addCustomWidget(QWidget *widget, bool stretch)
{
	m_layout->insertWidget(m_layout->count()-2, widget);
	if(stretch)
		m_layout->setStretchFactor(widget, 1);
}


void TitleWidget::addSpace(int space)
{
	m_layout->insertSpacing(m_layout->count()-2, space);
}

void TitleWidget::addStretch(int stretch)
{
	m_layout->insertStretch(m_layout->count()-2, stretch);
}

void TitleWidget::addCenteringSpacer()
{
	m_layout->insertStretch(0);
	m_layout->insertSpacing(0, m_closeButton->width() + m_dockButton->width());
	m_layout->insertStretch(m_layout->count()-2);
}

void TitleWidget::onFeaturesChanged(QDockWidget::DockWidgetFeatures features)
{
	m_dockButton->setEnabled(features.testFlag(QDockWidget::DockWidgetMovable));
	m_closeButton->setEnabled(features.testFlag(QDockWidget::DockWidgetClosable));
}

}
