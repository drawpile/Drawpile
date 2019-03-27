/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include <QObject>

/**
 * An event filter that emits hotBorder(bool) when mouse touches or leaves the top edge of the screen
 *
 * Absolute pointer coordinates are used. This can be used to show and hide the menu
 * bar in fullscreen mode. (Note: not needed on macOS, as the OS provides this feature itself.)
 */
class HotBorderEventFilter : public QObject {
	Q_OBJECT
public:
	HotBorderEventFilter(QObject *parent);

signals:
	void hotBorder(bool active);

protected:
	bool eventFilter(QObject *object, QEvent *event) override;

private:
	bool m_hotBorder;
};
