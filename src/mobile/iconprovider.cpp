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

#include "iconprovider.h"
#include "icon.h"

namespace icon {

IconProvider::IconProvider()
	: QQuickImageProvider(QQuickImageProvider::Pixmap)
{

}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
	Theme theme = CURRENT;

	int w = 32;
	int h = 32;

	if(size)
		*size = QSize(w, h);

	QString name;
	if(id.startsWith("light/")) {
		theme = LIGHT;
		name = id.mid(6);

	} else if(id.startsWith("dark/")) {
		theme = DARK;
		name = id.mid(5);

	} else {
		name = id;
	}

	QIcon icon = fromTheme(name, theme);

	return icon.pixmap(
		requestedSize.width() > 0 ? requestedSize.width() : w,
		requestedSize.height() > 0 ? requestedSize.height() : h);
}

}
