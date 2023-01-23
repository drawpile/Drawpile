/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2022 Calle Laakkonen

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

#include "libclient/utils/identicon.h"

#include <QPainter>

QImage make_identicon(const QString &name, const QSize &size)
{
	const auto hash = qHash(name);
	const QColor color = QColor::fromHsl(hash % 360, 165, 142);

	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(color);

	QPainter painter(&image);
	QFont font(QStringLiteral("sans"));
	font.setPixelSize(size.height() * 0.7);
	painter.setFont(font);
	painter.setPen(Qt::white);
	painter.drawText(QRect(QPoint(), size), Qt::AlignCenter, name.left(1));

	return image;
}

