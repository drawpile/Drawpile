// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/nonaltstealingmenubar.h"
#include "desktop/utils/qtguicompat.h"
#include <QApplication>
#include <QPointer>
#include <QProxyStyle>

namespace widgets {

namespace {

// See GroupedToolButton's style for comments, this is the same idea.
class NonAltStealingMenuBarStyle final : public QProxyStyle {
public:
	static NonAltStealingMenuBarStyle *instance()
	{
		if(!g_instance) {
			static Listener listener;
			qApp->installEventFilter(&listener);
			reset();
		}
		return g_instance;
	}

	using QProxyStyle::QProxyStyle;

	int styleHint(
		StyleHint hint, const QStyleOption *option = nullptr,
		const QWidget *widget = nullptr,
		QStyleHintReturn *returnData = nullptr) const override
	{
		if(hint == SH_MenuBar_AltKeyNavigation) {
			return 0;
		} else {
			return QProxyStyle::styleHint(hint, option, widget, returnData);
		}
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
		NonAltStealingMenuBarStyle *oldStyle = g_instance.data();

		QString themeStyle = compat::styleName(*QApplication::style());
		g_instance = new NonAltStealingMenuBarStyle(themeStyle);
		g_instance->setParent(qApp);

		for(QWidget *widget : qApp->allWidgets()) {
			if(NonAltStealingMenuBar *menuBar =
				   qobject_cast<NonAltStealingMenuBar *>(widget)) {
				menuBar->setStyle(g_instance);
			}
		}

		delete oldStyle;
	}

	static QPointer<NonAltStealingMenuBarStyle> g_instance;
};

QPointer<NonAltStealingMenuBarStyle> NonAltStealingMenuBarStyle::g_instance;

}

NonAltStealingMenuBar::NonAltStealingMenuBar(QWidget *parent)
	: QMenuBar(parent)
{
	setStyle(NonAltStealingMenuBarStyle::instance());
}

}
