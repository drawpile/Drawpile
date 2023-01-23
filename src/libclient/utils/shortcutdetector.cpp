/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "libclient/utils/shortcutdetector.h"

#include <QEvent>

ShortcutDetector::ShortcutDetector(QObject *parent)
	: QObject(parent), _shortcutSent(false)
{

}

bool ShortcutDetector::eventFilter(QObject *obj, QEvent *event)
{
	if(event->type() == QEvent::Shortcut) {
		_shortcutSent = true;
	}
	return QObject::eventFilter(obj, event);
}
