// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2016 The Qt Company Ltd.
// SPDX-FileComment: Based on QCommandLinkButton.
#include "desktop/widgets/commandlinkbutton.h"
#include <QFontMetrics>
#include <QIcon>
#include <QPalette>
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QTextLayout>
#include <QtMath>

namespace widgets {

CommandLinkButton::CommandLinkButton(QWidget *parent)
	: CommandLinkButton(QIcon(), QString(), QString(), parent)
{
}

CommandLinkButton::CommandLinkButton(
	const QIcon &icon, const QString &text, const QString &description,
	QWidget *parent)
	: QPushButton(icon, text, parent)
{
	setAttribute(Qt::WA_Hover);
	setAttribute(Qt::WA_MacShowFocusRect, false);

	QSizePolicy policy(
		QSizePolicy::Preferred, QSizePolicy::Preferred,
		QSizePolicy::PushButton);
	policy.setHeightForWidth(true);
	setSizePolicy(policy);

	setIconSize(QSize(20, 20));

	setDescription(description);
}

void CommandLinkButton::setDescription(const QString &description)
{
	if(description != m_description) {
		m_description = description;
		updateGeometry();
		update();
	}
}

QSize CommandLinkButton::sizeHint() const
{
	QSize size = QPushButton::sizeHint();
	QFontMetrics fm(titleFont());
	int textWidth = qMax(fm.horizontalAdvance(text()), 135);
	int buttonWidth = textWidth + textOffset() + MARGIN_RIGHT;
	int heightWithoutDescription = descriptionOffset() + MARGIN_BOTTOM;

	size.setWidth(qMax(size.width(), buttonWidth));
	size.setHeight(qMax(
		m_description.isEmpty() ? 41 : 60,
		heightWithoutDescription + descriptionHeight(buttonWidth)));

	return size;
}

int CommandLinkButton::heightForWidth(int width) const
{
	int heightWithoutDescription = descriptionOffset() + MARGIN_BOTTOM;
	return qMax(
		heightWithoutDescription + descriptionHeight(width),
		icon().actualSize(iconSize()).height() + MARGIN_TOP + MARGIN_BOTTOM);
}

QSize CommandLinkButton::minimumSizeHint() const
{
	QSize size = sizeHint();
	int minimumHeight = qMax(
		descriptionOffset() + MARGIN_BOTTOM,
		icon().actualSize(iconSize()).height() + MARGIN_TOP);
	size.setHeight(minimumHeight);
	return size;
}

void CommandLinkButton::paintEvent(QPaintEvent *)
{
	QStylePainter p(this);

	QStyleOptionButton option;
	initStyleOption(&option);
	option.features |= QStyleOptionButton::CommandLinkButton;
	option.text = QString();
	option.icon = QIcon();

	QStyle *s = style();
	int vOffset, hOffset;
	if(isDown()) {
		vOffset = s->pixelMetric(QStyle::PM_ButtonShiftVertical, &option, this);
		hOffset =
			s->pixelMetric(QStyle::PM_ButtonShiftHorizontal, &option, this);
	} else {
		vOffset = 0;
		hOffset = 0;
	}

	// Draw icon
	p.drawControl(QStyle::CE_PushButton, option);
	QIcon::Mode icnMode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
	QIcon::State icnState = isChecked() ? QIcon::On : QIcon::Off;
	if(QIcon icn = icon(); !icn.isNull()) {
		QSize icnSize = icn.actualSize(iconSize());
		QRect icnRect(
			MARGIN_LEFT + hOffset, MARGIN_TOP + vOffset, icnSize.width(),
			icnSize.height());
		icn.paint(&p, icnRect, Qt::AlignCenter, icnMode, icnState);
	}

	int textflags = Qt::TextShowMnemonic;
	if(!s->styleHint(QStyle::SH_UnderlineShortcut, &option, this)) {
		textflags |= Qt::TextHideMnemonic;
	}

	// Draw title
	if(QString title = text(); !title.isEmpty()) {
		QFont f = titleFont();
		QRect titleRect =
			rect().adjusted(textOffset(), MARGIN_TOP, -MARGIN_RIGHT, 0);
		if(m_description.isEmpty()) {
			QFontMetrics fm(f);
			titleRect.setTop(
				titleRect.top() +
				qMax(
					0, (icon().actualSize(iconSize()).height() - fm.height()) /
						   2));
		}

		p.setFont(f);
		p.drawItemText(
			titleRect.translated(hOffset, vOffset), textflags, option.palette,
			isEnabled(), text(), QPalette::ButtonText);
	}

	// Draw description
	if(!m_description.isEmpty()) {
		// The original QCommandLinkButton also adusts by MARGIN_BOTTOM here,
		// but that ends up cutting off text at the bottom on Chinese Windows
		// for some reason. Not sure why, but there seems to be various weird
		// layout and font hacks that Qt does there, so it's probably just
		// caused by that. The extra space makes them not get cut off anyway.
		QRect descriptionRect = rect().adjusted(
			textOffset(), descriptionOffset(), -MARGIN_RIGHT, 0);

		p.setFont(descriptionFont());
		p.drawItemText(
			descriptionRect.translated(hOffset, vOffset),
			textflags | Qt::TextWordWrap | Qt::ElideRight, option.palette,
			isEnabled(), m_description, QPalette::ButtonText);
	}
}

QFont CommandLinkButton::titleFont() const
{
	QFont f = font();
	f.setBold(true);
	return f;
}

QFont CommandLinkButton::descriptionFont() const
{
	return font();
}

int CommandLinkButton::textOffset() const
{
	return icon().actualSize(iconSize()).width() + MARGIN_LEFT + 6;
}

int CommandLinkButton::descriptionOffset() const
{
	QFontMetrics fm(titleFont());
	return MARGIN_TOP + fm.height();
}

int CommandLinkButton::descriptionHeight(int widgetWidth) const
{
	int lineWidth = widgetWidth - textOffset() - MARGIN_RIGHT;
	qreal descriptionHeight = 0;

	if(!m_description.isEmpty()) {
		QTextLayout layout(m_description);
		layout.setFont(descriptionFont());
		layout.beginLayout();

		while(true) {
			QTextLine line = layout.createLine();
			if(line.isValid()) {
				line.setLineWidth(lineWidth);
				line.setPosition(QPointF(0, descriptionHeight));
				descriptionHeight += line.height();
			} else {
				break;
			}
		}

		layout.endLayout();
	}

	return qCeil(descriptionHeight);
}

}
