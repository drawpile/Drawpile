// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/utils/qtguicompat.h"

#include <QApplication>
#include <QEvent>
#include <QPainterPath>
#include <QPointer>
#include <QProxyStyle>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>
#include <QWidget>

namespace widgets {

constexpr auto SWATCH_HEIGHT = 5;

// Using a proxy style ensures that all of the normal tool button control styles
// are rendered correctly and only the edges of the background are clipped.
// Trying to manually render a couple primitives does not work because at least
// the Qt macOS style does extra stuff inside of
// `drawComplexControl(QStyle::CC_ToolButton)` that is critical for correct
// rendering of the button.
class GroupedToolButtonStyle final : public QProxyStyle {
public:
	static GroupedToolButtonStyle *instance()
	{
		if (!g_instance) {
			static Listener listener;
			qApp->installEventFilter(&listener);
			reset();
		}
		return g_instance;
	}

	using QProxyStyle::QProxyStyle;

	QRect subControlRect(QStyle::ComplexControl control, const QStyleOptionComplex *option, QStyle::SubControl subControl, const QWidget *widget = nullptr) const override
	{
		auto rect = QProxyStyle::subControlRect(control, option, subControl, widget);
		if (control == QStyle::CC_ToolButton && subControl == QStyle::SC_ToolButton) {
			const auto *button = qobject_cast<const GroupedToolButton *>(widget);
			if (button && button->groupPosition()) {
				const auto overdraw = rect.width();
				if (button->groupPosition() & GroupedToolButton::GroupLeft) {
					rect.adjust(0, 0, overdraw, 0);
				}
				if (button->groupPosition() & GroupedToolButton::GroupRight) {
					rect.adjust(-overdraw, 0, 0, 0);
				}
			}
		}
		return rect;
	}

	void drawControl(QStyle::ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override
	{
		if (element == CE_ToolButtonLabel) {
			const auto *button = qobject_cast<const GroupedToolButton *>(widget);
			auto labelOption = *static_cast<const QStyleOptionToolButton *>(option);
			if (button && (button->groupPosition() || button->colorSwatch().isValid())) {
				const auto overdraw = labelOption.rect.width() /
					(button->groupPosition() == GroupedToolButton::GroupCenter ? 3 : 2);

				if (button->groupPosition() & GroupedToolButton::GroupLeft) {
					labelOption.rect.adjust(0, 0, -overdraw, 0);
				}
				if (button->groupPosition() & GroupedToolButton::GroupRight) {
					labelOption.rect.adjust(overdraw, 0, 0, 0);
				}
				if (button->colorSwatch().isValid()) {
					labelOption.rect.adjust(0, 0, 0, -SWATCH_HEIGHT);
				}
				return QProxyStyle::drawControl(element, &labelOption, painter, widget);
			}
		}
		return QProxyStyle::drawControl(element, option, painter, widget);
	}

private:
	class Listener final : public QObject {
	public:
		bool eventFilter(QObject *object, QEvent *event) override
		{
			if (object == qApp && event->type() == QEvent::StyleChange) {
				reset();
			}
			return QObject::eventFilter(object, event);
		}
	};

	static void reset()
	{
		// The old style has to live until all of the widgets that were using
		// it have been swapped over to the new style since they might use the
		// old style to unpolish themselves
		auto *oldStyle = g_instance.data();

		g_instance = new GroupedToolButtonStyle(compat::styleName(*QApplication::style()));
		g_instance->setParent(qApp);

		for (auto &widget : qApp->allWidgets()) {
			if (auto *button = qobject_cast<GroupedToolButton *>(widget)) {
				button->setStyle(g_instance);
			}
		}

		delete oldStyle;
	}

	static QPointer<GroupedToolButtonStyle> g_instance;
};

QPointer<GroupedToolButtonStyle> GroupedToolButtonStyle::g_instance;

GroupedToolButton::GroupedToolButton(QWidget *parent)
	: GroupedToolButton(NotGrouped, parent)
{
	setStyle(GroupedToolButtonStyle::instance());
}

GroupedToolButton::GroupedToolButton(GroupPosition position, QWidget* parent)
	: QToolButton(parent)
	, m_groupPosition(position)
{
	setFocusPolicy(Qt::NoFocus);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	setStyle(GroupedToolButtonStyle::instance());
}

void GroupedToolButton::setGroupPosition(GroupedToolButton::GroupPosition groupPosition)
{
	m_groupPosition = groupPosition;
	update();
}

void GroupedToolButton::setColorSwatch(const QColor &c)
{
	m_colorSwatch = c;
	update();
}

void GroupedToolButton::paintEvent(QPaintEvent *)
{
	QStylePainter painter(this);
	QStyleOptionToolButton opt;
	initStyleOption(&opt);

	painter.drawComplexControl(QStyle::CC_ToolButton, opt);

	if(m_colorSwatch.isValid()) {
		const auto swatchRect = QRect(
			opt.rect.x(), opt.rect.bottom() - SWATCH_HEIGHT,
			opt.rect.width(), SWATCH_HEIGHT
		);
		painter.fillRect(swatchRect, m_colorSwatch);
		opt.rect.setHeight(opt.rect.height() - SWATCH_HEIGHT);
	}

	// Separators
	const int y1 = opt.rect.top() + 5;
	const int y2 = opt.rect.bottom() - 5;
	// The palette roles used here are selected to match a `QFrame::VLine` with
	// `QFrame::Sunken` shadow
	if (m_groupPosition & GroupRight) {
		const int x = opt.rect.left();
		painter.setPen(opt.palette.color(QPalette::Light));
		painter.drawLine(x, y1, x, y2);
	}
	if (m_groupPosition & GroupLeft) {
		const int x = opt.rect.right();
		painter.setPen(opt.palette.color(QPalette::Dark));
		painter.drawLine(x, y1, x, y2);
	}
}

}
