/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "identicon.h"

#include <QPainter>
#include <QCryptographicHash>
#include <QtEndian>
#include <QtMath>

namespace {
	class HashRand {
	public:
		HashRand(const QByteArray &seed)
			: m_pos(0)
		{
			m_seed = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
		}

		unsigned int operator()(unsigned int bound)
		{
			if(m_pos+4 > m_seed.length()) {
				m_pos = 0;
				m_seed = QCryptographicHash::hash(m_seed, QCryptographicHash::Sha256);
			} else {
				m_pos += 4;
			}

			return qFromBigEndian<quint32>(m_seed.constData()+m_pos) % bound;
		}

	private:
		QByteArray m_seed;
		int m_pos;
	};
}

QImage make_identicon(const QString &name, const QSize &size)
{
	HashRand rand {name.toUtf8()};

	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(QColor::fromHsl(rand(360), 255, 120));

	// Draw the decorative elements
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	const double offset = rand(180) / M_PI;
	for(int i=0;i<3;++i) {
		const QPointF c { size.width() / 2.0, size.height() / 2.0 };
		const double r = size.width() * 2;
		painter.setBrush(QColor::fromHsl(rand(360), 255, 180, 255/2));
		painter.drawEllipse(QRectF(
			c.x() + sin(offset + i/3.0 * M_PI*2) * r - r,
			c.y() + cos(offset + i/3.0 * M_PI*2) * r - r,
			r*2,
			r*2
		));
	}

	// Draw the name's first letter
	QFont font;
	font.setPixelSize(size.height() * 0.7);
	painter.setFont(font);
	painter.setPen(Qt::white);
	painter.drawText(QRect(QPoint(), size), Qt::AlignCenter, name.left(1));

	// Draw the circular mask
	QImage mask(size, QImage::Format_ARGB32_Premultiplied);
	mask.fill(0);
	QPainter maskPainter(&mask);
	maskPainter.setRenderHint(QPainter::Antialiasing);
	maskPainter.setBrush(Qt::white);
	maskPainter.drawEllipse(1, 1, size.width()-2, size.height()-2);

	painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	painter.drawImage(0, 0, mask);

	return image;
}

