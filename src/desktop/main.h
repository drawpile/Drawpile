/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#ifndef DRAWPILEAPP_H
#define DRAWPILEAPP_H

#include <QApplication>
#include <QMap>
#include "notifications.h"

class QSoundEffect;

class DrawpileApp : public QApplication {
Q_OBJECT
   friend void notification::playSound(notification::Event);
public:
   enum Theme {
	  THEME_SYSTEM,
	  THEME_FUSION_LIGHT,
	  THEME_FUSION_DARK,
	  THEME_COUNT,
	  // On OSX, there's no theme selection. The settings code says something
	  // about it being broken prior to Qt5.12, but I have no way of checking
	  // if it works now. So we'll just always use the system theme there.
#ifdef Q_OS_MAC
	  THEME_DEFAULT = THEME_SYSTEM,
#else
	  THEME_DEFAULT = THEME_FUSION_LIGHT,
#endif
   };

	DrawpileApp(int & argc, char ** argv );
   virtual ~DrawpileApp();

	void setTheme(int theme);
	void notifySettingsChanged();

	void openUrl(QUrl url);

	void openBlankDocument();

signals:
	void settingsChanged();
	void eraserNear(bool near);

protected:
	bool event(QEvent *e);

private:
	QMap<notification::Event, QSoundEffect*> m_sounds;

	QPalette loadPalette(const QString &file);
};

#endif
