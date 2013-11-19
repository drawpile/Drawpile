/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_NET_CLIENT_H
#define DP_NET_CLIENT_H

#include <QObject>

#include "core/point.h"

namespace dpcore {
	class Point;
}

namespace protocol {
	class Message;
}

namespace drawingboard {
	class ToolContext;
}

namespace net {
	
class Server;
class LoopbackServer;

/**
 * The client for accessing the drawing server.
 */
class Client : public QObject {
Q_OBJECT
public:
	Client(QObject *parent=0);
	~Client();

	//! Reinitialize after clearing out the old board
	void init();

	// Layer changing
	void sendCanvasResize(const QSize &newsize);
	void sendNewLayer(int id, const QColor &fill, const QString &title);
	void sendLayerAttribs(int id, float opacity, const QString &title);
	void sendLayerReorder(const QList<uint8_t> &ids);
	void sendDeleteLayer(int id, bool merge);

	// Drawing
	void sendToolChange(const drawingboard::ToolContext &ctx);
	void sendStroke(const dpcore::Point &point);
	void sendStroke(const dpcore::PointVector &points);
	void sendPenup();
	void sendImage(int layer, int x, int y, const QImage &image, bool blend);

	// Annotations
	void sendAnnotationCreate(int id, const QRect &rect);
	void sendAnnotationReshape(int id, const QRect &rect);
	void sendAnnotationEdit(int id, const QColor &bg, const QString &text);
	void sendAnnotationDelete(int id);

signals:
	void drawingCommandReceived(protocol::Message *msg);

protected slots:
	void handleMessage(protocol::Message *msg);

private:
	Server *_server;
	LoopbackServer *_loopback;
	
	int _my_id;
};

}

#endif
