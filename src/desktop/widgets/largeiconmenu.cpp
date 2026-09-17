// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/largeiconmenu.h"
#include "desktop/utils/qtguicompat.h"
#include <QApplication>
#include <QPointer>
#include <QProxyStyle>
#ifdef HAVE_PROXY_STYLE
#	include "desktop/utils/fusionui.h"
#endif

namespace widgets {

// See groupedtoolbutton.cpp, similar kind of deal.
class LargeIconMenuStyle final : public QProxyStyle {
public:
	static LargeIconMenuStyle *instance()
	{
		if(!g_instance) {
			static Listener listener;
			qApp->installEventFilter(&listener);
			reset();
		}
		return g_instance;
	}

	using QProxyStyle::QProxyStyle;

	int pixelMetric(
		PixelMetric metric, const QStyleOption *option = nullptr,
		const QWidget *widget = nullptr) const override
	{
		// QMenu always uses small icon sizes, force it to use large ones.
		return QProxyStyle::pixelMetric(
			metric == PM_SmallIconSize ? PM_ToolBarIconSize : metric, option,
			widget);
	}

private:
	class Listener final : public QObject {
	public:
		bool eventFilter(QObject *object, QEvent *event) override
		{
			if(object == qApp && event->type() == QEvent::StyleChange) {
				reset();
			}
			return QObject::eventFilter(object, event);
		}
	};

	static void reset()
	{
		LargeIconMenuStyle *oldStyle = g_instance.data();

		QString themeStyle = compat::styleName(*QApplication::style());
#ifdef HAVE_PROXY_STYLE
		if(fusionui::looksLikeFusionStyle(themeStyle)) {
			g_instance = new LargeIconMenuStyle(
				new fusionui::FusionProxyStyle(themeStyle));
		} else {
			g_instance = new LargeIconMenuStyle(themeStyle);
		}
#else
		g_instance = new LargeIconMenuStyle(themeStyle);
#endif
		g_instance->setParent(qApp);

		for(QWidget *widget : qApp->allWidgets()) {
			if(LargeIconMenu *menu = qobject_cast<LargeIconMenu *>(widget)) {
				menu->setStyle(g_instance);
			}
		}

		delete oldStyle;
	}

	static QPointer<LargeIconMenuStyle> g_instance;
};

QPointer<LargeIconMenuStyle> LargeIconMenuStyle::g_instance;

LargeIconMenu::LargeIconMenu(QWidget *parent)
	: QMenu(parent)
{
	setStyle(LargeIconMenuStyle::instance());
}

}
