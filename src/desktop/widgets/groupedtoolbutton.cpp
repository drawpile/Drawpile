// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/utils/icon.h"

#include <QAction>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>
#include <QEvent>

namespace widgets
{

GroupedToolButton::GroupedToolButton(QWidget *parent) : GroupedToolButton(NotGrouped, parent) { }

#ifdef Q_OS_MACOS
static const QString LIGHT_STYLE = QStringLiteral(
			"QToolButton {"
				"background: white;"
				"border: 1px solid #c0c0c0;"
				"border-radius: 3px;"
				"padding: 1px"
			"}"
			"QToolButton:checked, QToolButton:pressed {"
				"background: #c0c0c0"
			"}"
		);

static const QString DARK_STYLE = QStringLiteral(
			"QToolButton {"
				"background: #656565;"
				"border-radius: 3px;"
				"padding: 1px"
			"}"
			"QToolButton:checked, QToolButton:pressed {"
				"background: #999"
			"}"
		);
#endif

GroupedToolButton::GroupedToolButton(GroupPosition position, QWidget* parent)
: QToolButton(parent), mGroupPosition(position)
{
	setFocusPolicy(Qt::NoFocus);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
#ifdef Q_OS_MACOS
	if(icon::isDark(palette().color(QPalette::Window)))
		setStyleSheet(DARK_STYLE);
	else
		setStyleSheet(LIGHT_STYLE);
#endif
}

void GroupedToolButton::setGroupPosition(GroupedToolButton::GroupPosition groupPosition)
{
	mGroupPosition = groupPosition;
}

void GroupedToolButton::setColorSwatch(const QColor &c)
{
	m_colorSwatch = c;
	update();
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

	// Color swatch (if set)
	if(m_colorSwatch.isValid()) {
		const int swatchH = 5;
		const QRect swatchRect = QRect(opt.rect.x(), opt.rect.bottom()-swatchH, opt.rect.width(), swatchH);
		painter.fillRect(swatchRect, m_colorSwatch);
		opt.rect.setHeight(opt.rect.height() - swatchH);
	}

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

	const bool showDropdownArrow = menu() != nullptr && !text().isEmpty();

	QRect textRect = opt.rect;
	QRect arrowRect;

	if(showDropdownArrow) {
		arrowRect = QRect(textRect.right() - 20, textRect.y(), 20, textRect.height());
		textRect.setWidth(textRect.width() - arrowRect.width());
	}

	// Text
	opt.rect = textRect;
	painter.drawControl(QStyle::CE_ToolButtonLabel, opt);

	// Dropdown arrow
	if(showDropdownArrow) {
		opt.rect = arrowRect;
		painter.drawPrimitive(QStyle::PE_IndicatorArrowDown, opt);
	}
}

}

