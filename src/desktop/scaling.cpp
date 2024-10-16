// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scaling.h"
#include "libclient/utils/scopedoverridecursor.h"
#include <QAction>
#include <QApplication>
#include <QHash>
#include <QLoggingCategory>
#include <QMenu>
#include <QSignalBlocker>
#include <QString>
#include <QtGui/private/qhighdpiscaling_p.h>

Q_LOGGING_CATEGORY(lcDpScaling, "net.drawpile.scaling", QtDebugMsg)

namespace scaling {

static constexpr char SCALE_FACTOR_PROPERTY[] = "drawpile_scale_factor";
static qreal overrideScaleFactor;
static QHash<QString, qreal> *screenScaleFactorsByName;

static bool haveOverrideScaleFactor()
{
	return overrideScaleFactor >= 1.0;
}

static void saveScreenFactor(QScreen *screen, qreal factor)
{
	QString name = screen->name();
	qCDebug(
		lcDpScaling, "Save scale factor %f of screen %p '%s'", factor,
		static_cast<void *>(screen), qUtf8Printable(name));
	screen->setProperty(SCALE_FACTOR_PROPERTY, factor);
	if(!name.isEmpty()) {
		screenScaleFactorsByName->insert(name, factor);
	}
}

static void
setScreenFactorFrom(QScreen *screen, qreal factor, const char *source)
{
	qreal effectiveFactor = qMax(1.0, factor);
	qCDebug(
		lcDpScaling,
		"Set factor of screen %p '%s' from %s to %f (effectively %f)",
		static_cast<void *>(screen), qUtf8Printable(screen->name()), source,
		factor, effectiveFactor);
	QHighDpiScaling::setScreenFactor(screen, effectiveFactor);
}

static bool loadScreenFactor(const QScreen *screen, qreal &outFactor)
{
	QString name = screen->name();
	if(!name.isEmpty()) {
		QHash<QString, qreal>::const_iterator it =
			screenScaleFactorsByName->constFind(name);
		if(it != screenScaleFactorsByName->constEnd()) {
			outFactor = it.value();
			return true;
		}
	}

	QVariant prop = screen->property(SCALE_FACTOR_PROPERTY);
	if(prop.isValid()) {
		bool ok;
		qreal factor = prop.toReal(&ok);
		if(ok) {
			outFactor = factor;
			return true;
		}
	}

	outFactor = getScreenScale(screen);
	return false;
}

static void addScreen(QScreen *screen)
{
	QString name = screen->name();
	qCDebug(
		lcDpScaling, "Screen %p '%s' added", static_cast<void *>(screen),
		qUtf8Printable(name));

	qreal factor;
	bool loaded = loadScreenFactor(screen, factor);
	saveScreenFactor(screen, factor);

	if(haveOverrideScaleFactor()) {
		setScreenFactorFrom(screen, overrideScaleFactor, "override");
	} else {
		setScreenFactorFrom(
			screen, factor, loaded ? "previous value" : "own value");
	}
}

void initScaling(qreal initialOverrideFactor)
{
	Q_ASSERT(!screenScaleFactorsByName);
	overrideScaleFactor = initialOverrideFactor;
	screenScaleFactorsByName = new QHash<QString, qreal>;
	for(QScreen *screen : QGuiApplication::screens()) {
		qreal factor = getScreenScale(screen);
		saveScreenFactor(screen, factor);
		if(haveOverrideScaleFactor()) {
			setScreenFactorFrom(
				screen, overrideScaleFactor, "initial override");
		} else {
			setScreenFactorFrom(screen, factor, "own initial value");
		}
	}
	QObject::connect(qApp, &QApplication::screenAdded, &addScreen);
}

qreal getScreenScale(const QScreen *screen)
{
	return screen ? QHighDpiScaling::scaleAndOrigin(screen).factor : 0.0;
}

static qreal getIntendedScreenScale(QScreen *screen)
{
	qreal factor;
	if(haveOverrideScaleFactor()) {
		factor = overrideScaleFactor;
	} else if(!loadScreenFactor(screen, factor)) {
		qCWarning(
			lcDpScaling, "No intended scale factor for screen %p '%s'",
			static_cast<void *>(screen), qUtf8Printable(screen->name()));
	}
	return qMax(1.0, factor);
}

static void updateScreenScaleFactors()
{
	utils::ScopedOverrideCursor overrideCursor;

	QVector<QPair<QWindow *, QSize>> windowsToShow;
	for(QWindow *win : QGuiApplication::topLevelWindows()) {
		QScreen *screen = win->screen();
		if(screen && win->isVisible() &&
		   getScreenScale(screen) != getIntendedScreenScale(screen)) {
			QSize size = win->size();
			windowsToShow.append({win, size});
			win->hide();
			win->resize(size.width() + 1, size.height() + 1);
		}
	}
	QCoreApplication::processEvents();

	QVector<QScreen *> changedScreens;
	const char *source =
		haveOverrideScaleFactor() ? "override factor" : "screen factor";
	for(QScreen *screen : QGuiApplication::screens()) {
		qreal factor = getIntendedScreenScale(screen);
		if(getScreenScale(screen) != factor) {
			changedScreens.append(screen);
			setScreenFactorFrom(screen, factor, source);
			QCoreApplication::processEvents();
		}
	}

	QCoreApplication::processEvents();
	for(QWindow *win : QGuiApplication::allWindows()) {
		QScreen *screen = win->screen();
		if(screen && changedScreens.contains(screen)) {
			emit win->screenChanged(screen);
			QCoreApplication::processEvents();
		}
	}

	for(const QPair<QWindow *, QSize> &p : windowsToShow) {
		p.first->resize(p.second);
		p.first->show();
		QCoreApplication::processEvents();
	}

	// Menus fail to account for the changed size unless modified and shown, no
	// way around it. So we add garbage to every menu and flicker it visible.
	QAction *dummy = new QAction;
	dummy->setEnabled(false);
	for(QWidget *widget : qApp->topLevelWidgets()) {
		QScreen *screen = widget->screen();
		if(screen && changedScreens.contains(screen)) {
			for(QMenu *menu : widget->findChildren<QMenu *>()) {
				QSignalBlocker blocker(menu);
				menu->addAction(dummy);
				menu->show();
				QCoreApplication::processEvents();
				menu->hide();
				menu->removeAction(dummy);
			}
		}
	}
	QCoreApplication::processEvents();
	dummy->deleteLater();
}

qreal getOverrideFactor()
{
	return haveOverrideScaleFactor() ? overrideScaleFactor : 0.0;
}

void setOverrideFactor(qreal newOverrideFactor)
{
	bool hadOverrideFactor = haveOverrideScaleFactor();
	qreal oldOverrideFactor = overrideScaleFactor;
	overrideScaleFactor = newOverrideFactor;
	bool needsUpdate = haveOverrideScaleFactor() != hadOverrideFactor ||
					   oldOverrideFactor != overrideScaleFactor;
	if(needsUpdate) {
		updateScreenScaleFactors();
	}
}

qreal getWidgetScale(QWidget *widget)
{
	if(haveOverrideScaleFactor()) {
		return overrideScaleFactor;
	} else {
		QScreen *screen = widget ? widget->screen() : nullptr;
		return qMax(
			1.0,
			getScreenScale(screen ? screen : QApplication::primaryScreen()));
	}
}

}
