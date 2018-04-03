/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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

#include "commands.h"

#include "../shared/net/control.h"
#include "../shared/net/image.h"

#include <QImage>

namespace net {
namespace command {

namespace {

// Check if the given image consists entirely of fully transparent pixels
bool isEmptyImage(const QImage &image)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);
	const quint32 *pixels = reinterpret_cast<const quint32*>(image.bits());
	const quint32 *end = pixels + image.width()*image.height();
	while(pixels<end) {
		if(*(pixels++))
			return false;
	}
	return true;
}

// Recursively split image into small enough pieces.
// When mode is anything else than MODE_REPLACE, PutImage calls
// are expensive, so we want to split the image into as few pieces as possible.
void splitImage(int ctxid, int layer, int x, int y, const QImage &image, int mode, bool skipempty, QList<protocol::MessagePtr> &list)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);

	if(skipempty && isEmptyImage(image))
		return;

	// Compress pixel data and see if it fits in a single message
	const QByteArray data = QByteArray::fromRawData(reinterpret_cast<const char*>(image.bits()), image.byteCount());
	QByteArray compressed = qCompress(data);

	if(compressed.length() > protocol::PutImage::MAX_LEN) {
		// Too big! Recursively divide the image and try sending those
		compressed = QByteArray(); // release data

		QImage i1, i2;
		int px, py;
		if(image.width() > image.height()) {
			px = image.width() / 2;
			py = 0;
			i1 = image.copy(0, 0, px, image.height());
			i2 = image.copy(px, 0, image.width()-px, image.height());
		} else {
			px = 0;
			py = image.height() / 2;
			i1 = image.copy(0, 0, image.width(), py);
			i2 = image.copy(0, py, image.width(), image.height()-py);
		}

		splitImage(ctxid, layer, x, y, i1, mode, skipempty, list);
		splitImage(ctxid, layer, x+px, y+py, i2, mode, skipempty, list);

	} else {
		// It fits! Send data!
		list.append(protocol::MessagePtr(new protocol::PutImage(
			ctxid,
			layer,
			mode,
			x,
			y,
			image.width(),
			image.height(),
			compressed
		)));
	}
}
} // End anonymous namespace

using namespace protocol;

MessagePtr serverCommand(const QString &cmd, const QJsonArray &args, const QJsonObject &kwargs)
{
	protocol::ServerCommand c { cmd, args, kwargs };
	return MessagePtr(new Command(0, c));
}

MessagePtr kick(int target, bool ban)
{
	Q_ASSERT(target>0 && target<256);
	QJsonObject kwargs;
	if(ban)
		kwargs["ban"] = true;

	return serverCommand("kick-user", QJsonArray() << target, kwargs);
}

MessagePtr announce(const QString &url, bool privateMode)
{
	QJsonObject kwargs;
	if(privateMode)
		kwargs["private"] = true;

	return serverCommand("announce-session", QJsonArray() << url, kwargs);
}

MessagePtr unannounce(const QString &url)
{
	return serverCommand("unlist-session", QJsonArray() << url);
}

MessagePtr unban(int entryid)
{
	return serverCommand("remove-ban", QJsonArray() << entryid);
}

MessagePtr mute(int userId, bool mute)
{
	return serverCommand("mute", QJsonArray() << userId << mute);
}

MessagePtr terminateSession()
{
	return serverCommand("kill-session");
}

QList<protocol::MessagePtr> putQImage(int ctxid, int layer, int x, int y, QImage image, paintcore::BlendMode::Mode mode, bool skipempty)
{
	QList<protocol::MessagePtr> list;

	// Crop image if target coordinates are negative, since the protocol
	// does not support negative coordites.
	if(x<0 || y<0) {
		if(x < -image.width() || y < -image.height()) {
			// the entire image is outside the canvas
			return list;
		}

		int xoffset = x<0 ? -x : 0;
		int yoffset = y<0 ? -y : 0;
		image = image.copy(xoffset, yoffset, image.width()-xoffset, image.height()-yoffset);
		x += xoffset;
		y += yoffset;
	}

	image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	splitImage(ctxid, layer, x, y, image, mode, skipempty, list);

#ifndef NDEBUG
	if(list.isEmpty()) {
		qDebug("Not sending a %dx%d image because it was completely empty!", image.width(), image.height());
	} else {
		const double truesize = image.width() * image.height() * 4;
		double wiresize = 0;
		for(protocol::MessagePtr msg : list)
			wiresize += msg->length();
		qDebug("Sending a %.2f kb image in %d piece%s. Compressed size is %.2f kb. Compression ratio is %.1f%%",
			truesize/1024, list.size(), list.size()==1?"":"s", wiresize/1024, truesize/wiresize*100);
	}
#endif
	return list;
}

}
}
