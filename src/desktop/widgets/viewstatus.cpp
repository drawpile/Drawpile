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
	m_viewFlip = new QToolButton(this);
	m_viewFlip->setAutoRaise(true);

	m_viewMirror = new QToolButton(this);
	m_viewMirror->setAutoRaise(true);

	layout->addWidget(m_viewFlip);
	layout->addWidget(m_viewMirror);

	// Zoom level
	layout->addSpacing(10);
	m_zoomBox = new QComboBox(this);
	m_zoomBox->setFixedWidth(m_zoomBox->fontMetrics().width("9999.9%--"));
	m_zoomBox->setFrame(false);
	m_zoomBox->setEditable(true);
	m_zoomBox->setToolTip(tr("Zoom"));

	auto zoomBoxPalette = m_zoomBox->palette();
	zoomBoxPalette.setColor(QPalette::Base, zoomBoxPalette.color(QPalette::Window));
	m_zoomBox->setPalette(zoomBoxPalette);

	layout->addWidget(m_zoomBox);

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

void ViewStatus::setFlipActions(QAction *flip, QAction *mirror)
{
	m_viewFlip->setDefaultAction(flip);
	m_viewMirror->setDefaultAction(mirror);
}

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	Q_UNUSED(angle);
	const int intZoom = qRound(zoom);
	const int cursorPos = m_zoomBox->lineEdit()->cursorPosition();
	m_zoomBox->setEditText(QString::number(intZoom) + QChar('%'));
	m_zoomBox->lineEdit()->setCursorPosition(cursorPos);
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

