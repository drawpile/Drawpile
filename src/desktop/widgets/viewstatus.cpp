/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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

#include "viewstatus.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHBoxLayout>
#include <QAction>
#include <QToolButton>

namespace widgets {

ViewStatus::ViewStatus(QWidget *parent)
	: QWidget(parent)
{
#ifdef Q_OS_MAC
	setStyleSheet(QStringLiteral(
		"QToolButton { border: none }"
		"QToolButton:checked, QToolButton:pressed { background: #c0c0c0 }"
	));
#endif
	setMinimumHeight(22);
	QHBoxLayout *layout = new QHBoxLayout(this);

	layout->setMargin(1);
	layout->setSpacing(0);

	// View flipping
	layout->addSpacing(10);
	m_viewFlip = new QToolButton(this);
	m_viewFlip->setAutoRaise(true);

	m_viewMirror = new QToolButton(this);
	m_viewMirror->setAutoRaise(true);

	layout->addWidget(m_viewFlip);
	layout->addWidget(m_viewMirror);

	// Zoom level
	m_zoomReset = new QToolButton(this);
	m_zoomReset->setAutoRaise(true);

	m_zoomSlider = new QSlider(Qt::Horizontal, this);
	m_zoomSlider->setMaximumWidth(120);
	m_zoomSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
	m_zoomSlider->setMinimum(50);
	m_zoomSlider->setMaximum(1600);
	m_zoomSlider->setPageStep(50);
	m_zoomSlider->setValue(100);
	connect(m_zoomSlider, &QSlider::valueChanged, this, &ViewStatus::zoomChanged);

	m_zoomBox = new QComboBox(this);
	m_zoomBox->setFixedWidth(m_zoomBox->fontMetrics().width("9999.9%--"));
	m_zoomBox->setFrame(false);
	m_zoomBox->setEditable(true);

	auto zoomBoxPalette = m_zoomBox->palette();
	zoomBoxPalette.setColor(QPalette::Base, zoomBoxPalette.color(QPalette::Window));
	m_zoomBox->setPalette(zoomBoxPalette);

	layout->addWidget(m_zoomBox);
	layout->addWidget(m_zoomSlider);
	layout->addWidget(m_zoomReset);

	m_zoomBox->addItem(QStringLiteral("50%"));
	m_zoomBox->addItem(QStringLiteral("100%"));
	m_zoomBox->addItem(QStringLiteral("200%"));
	m_zoomBox->addItem(QStringLiteral("400%"));
	m_zoomBox->addItem(QStringLiteral("800%"));
	m_zoomBox->addItem(QStringLiteral("1600%"));
	m_zoomBox->setEditText(QStringLiteral("100%"));

	m_zoomBox->lineEdit()->setValidator(
		new QRegularExpressionValidator(
			QRegularExpression("[0-9]{0,4}%?"),
			this
		)
	);
	connect(m_zoomBox, &QComboBox::editTextChanged, this, &ViewStatus::zoomBoxChanged);
}

void ViewStatus::setZoomActions(QAction *reset)
{
	m_zoomReset->setDefaultAction(reset);
}

void ViewStatus::setFlipActions(QAction *flip, QAction *mirror)
{
	m_viewFlip->setDefaultAction(flip);
	m_viewMirror->setDefaultAction(mirror);
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	Q_UNUSED(angle);
	const int intZoom = qRound(zoom);
	if(intZoom != m_zoomSlider->value()) {
		const int cursorPos = m_zoomBox->lineEdit()->cursorPosition();
		m_zoomSlider->setValue(intZoom);
		m_zoomBox->setEditText(QString::number(intZoom) + QChar('%'));
		m_zoomBox->lineEdit()->setCursorPosition(cursorPos);
	}
}

void ViewStatus::zoomBoxChanged(const QString &text)
{
	const int suffix = text.indexOf('%');
	const QStringRef num = suffix>0 ? text.leftRef(suffix) : &text;

	bool ok;
	const int number = num.toInt(&ok);
	if(ok && number>= 1 && number < 10000)
		emit zoomChanged(number);
}

}

