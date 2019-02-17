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
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHBoxLayout>

namespace widgets {

ViewStatus::ViewStatus(QWidget *parent)
	: QWidget(parent), m_updating(false)
{
	setMinimumHeight(22);
	QHBoxLayout *layout = new QHBoxLayout(this);

	layout->setMargin(1);
	layout->setSpacing(0);

	// Canvas rotation
	m_angleBox = new QComboBox(this);
	m_angleBox->setFixedWidth(m_angleBox->fontMetrics().width("9999-O--"));
	m_angleBox->setFrame(false);
	m_angleBox->setEditable(true);
	m_angleBox->setToolTip(tr("Canvas Rotation"));

	auto boxPalette = m_angleBox->palette();
	boxPalette.setColor(QPalette::Base, boxPalette.color(QPalette::Window));
	m_angleBox->setPalette(boxPalette);

	layout->addWidget(m_angleBox);

	m_angleBox->addItem(QStringLiteral("-90°"));
	m_angleBox->addItem(QStringLiteral("-45°"));
	m_angleBox->addItem(QStringLiteral("0°"));
	m_angleBox->addItem(QStringLiteral("45°"));
	m_angleBox->addItem(QStringLiteral("90°"));
	m_angleBox->setEditText(QStringLiteral("0°"));

	m_angleBox->lineEdit()->setValidator(
		new QRegularExpressionValidator(
			QRegularExpression("-?[0-9]{0,3}°?"),
			this
		)
	);
	connect(m_angleBox, &QComboBox::editTextChanged, this, &ViewStatus::angleBoxChanged);

	// Zoom level
	m_zoomBox = new QComboBox(this);
	m_zoomBox->setFixedWidth(m_zoomBox->fontMetrics().width("9999.9%--"));
	m_zoomBox->setFrame(false);
	m_zoomBox->setEditable(true);
	m_zoomBox->setToolTip(tr("Zoom"));

	m_zoomBox->setPalette(boxPalette);

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

void ViewStatus::setTransformation(qreal zoom, qreal angle)
{
	m_updating = true;
	const int intZoom = qRound(zoom);
	const int zoomCursorPos = m_zoomBox->lineEdit()->cursorPosition();
	m_zoomBox->setEditText(QString::number(intZoom) + QChar('%'));
	m_zoomBox->lineEdit()->setCursorPosition(zoomCursorPos);

	const int intAngle = qRound(angle);
	const int angleCursorPos = m_angleBox->lineEdit()->cursorPosition();
	m_angleBox->setEditText(QString::number(intAngle) + QChar(0x00b0));
	m_angleBox->lineEdit()->setCursorPosition(angleCursorPos);
	m_updating = false;
}

void ViewStatus::zoomBoxChanged(const QString &text)
{
	if(m_updating)
		return;

	const int suffix = text.indexOf('%');
	const QStringRef num = suffix>0 ? text.leftRef(suffix) : &text;

	bool ok;
	const int number = num.toInt(&ok);
	if(ok && number>= 1 && number < 10000)
		emit zoomChanged(number);
}

void ViewStatus::angleBoxChanged(const QString &text)
{
	if(m_updating)
		return;

	const int suffix = text.indexOf(0x00b0);
	const QStringRef num = suffix>0 ? text.leftRef(suffix) : &text;

	bool ok;
	const int number = num.toInt(&ok);
	if(ok)
		emit angleChanged(number);
}

}
