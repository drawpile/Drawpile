/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen, GroupedToolButton based on Gwenview's StatusBarToolbutton by Aurélien Gâteau

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
#include "groupedtoolbutton.h"

#include <QAction>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>

#ifndef DESIGNER_PLUGIN
namespace widgets
{
#endif

GroupedToolButton::GroupedToolButton(QWidget *parent) : GroupedToolButton(NotGrouped, parent) { }

GroupedToolButton::GroupedToolButton(GroupPosition position, QWidget* parent)
: QToolButton(parent)
, mGroupPosition(position)
{
	setFocusPolicy(Qt::NoFocus);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void GroupedToolButton::setGroupPosition(GroupedToolButton::GroupPosition groupPosition)
{
	mGroupPosition = groupPosition;
}

void GroupedToolButton::paintEvent(QPaintEvent* event)
{
	if (mGroupPosition == NotGrouped) {
		QToolButton::paintEvent(event);
		return;
	}
	QStylePainter painter(this);
	QStyleOptionToolButton opt;
	initStyleOption(&opt);
	QStyleOptionToolButton panelOpt = opt;

	// Panel
	QRect& panelRect = panelOpt.rect;
	switch (mGroupPosition) {
	case GroupLeft:
		panelRect.setWidth(panelRect.width() * 2);
		break;
	case GroupCenter:
		panelRect.setLeft(panelRect.left() - panelRect.width());
		panelRect.setWidth(panelRect.width() * 3);
		break;
	case GroupRight:
		panelRect.setLeft(panelRect.left() - panelRect.width());
		break;
	case NotGrouped:
		Q_ASSERT(0);
	}
	painter.drawPrimitive(QStyle::PE_PanelButtonTool, panelOpt);

	// Separator
	const int y1 = opt.rect.top() + 6;
	const int y2 = opt.rect.bottom() - 6;
	if (mGroupPosition & GroupRight) {
		const int x = opt.rect.left();
		painter.setPen(opt.palette.color(QPalette::Light));
		painter.drawLine(x, y1, x, y2);
	}
	if (mGroupPosition & GroupLeft) {
		const int x = opt.rect.right();
		painter.setPen(opt.palette.color(QPalette::Mid));
		painter.drawLine(x, y1, x, y2);
	}

	// Text
	painter.drawControl(QStyle::CE_ToolButtonLabel, opt);
}

#ifndef DESIGNER_PLUGIN
}
#endif
