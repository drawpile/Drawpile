// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/io/activitybroadcast.h"
#include "libclient/brushes/brush.h"
#include "libclient/tools/tool.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QUdpSocket>

namespace io {

namespace {
static QJsonObject pointfToJson(const QPointF &p)
{
	return QJsonObject({
		{QStringLiteral("x"), p.x()},
		{QStringLiteral("y"), p.y()},
	});
}

static QJsonObject rectfToJson(const QRectF &r)
{
	return QJsonObject({
		{QStringLiteral("x"), r.x()},
		{QStringLiteral("y"), r.y()},
		{QStringLiteral("w"), r.width()},
		{QStringLiteral("h"), r.height()},
	});
}
}

ActivityBroadcast::ActivityBroadcast(QObject *parent)
	: QObject(parent)
{
}

bool ActivityBroadcast::start(int port, QString *outErrorMessage)
{
	if(m_socket) {
		if(outErrorMessage) {
			*outErrorMessage = QStringLiteral("");
		}
		return false;
	}

	m_socket = new QUdpSocket(this);
	if(!m_socket->open(QIODevice::WriteOnly)) {
		*outErrorMessage = QStringLiteral("Error %1 opening UDP socket: %2")
							   .arg(int(m_socket->error()))
							   .arg(m_socket->errorString());
		delete m_socket;
		m_socket = nullptr;
		return false;
	}

	m_port = port;
	return true;
}

bool ActivityBroadcast::stop()
{
	if(m_socket) {
		delete m_socket;
		m_socket = nullptr;
		m_port = -1;
		return true;
	} else {
		return false;
	}
}

void ActivityBroadcast::sendActiveBrush(const brushes::ActiveBrush &brush)
{
	sendData(
		QStringLiteral("brush"),
		QJsonObject({
			{QStringLiteral("eraser"), brush.isEraser()},
		}));
}

void ActivityBroadcast::sendActiveTool(int type)
{
	QString name;
	switch(tools::Tool::Type(type)) {
	case tools::Tool::Type::FREEHAND:
		name = QStringLiteral("freehand");
		break;
	case tools::Tool::Type::ERASER:
		name = QStringLiteral("eraser");
		break;
	case tools::Tool::Type::LINE:
		name = QStringLiteral("line");
		break;
	case tools::Tool::Type::RECTANGLE:
		name = QStringLiteral("rectangle");
		break;
	case tools::Tool::Type::ELLIPSE:
		name = QStringLiteral("ellipse");
		break;
	case tools::Tool::Type::BEZIER:
		name = QStringLiteral("bezier");
		break;
	case tools::Tool::Type::FLOODFILL:
		name = QStringLiteral("floodfill");
		break;
	case tools::Tool::Type::LASSOFILL:
		name = QStringLiteral("lassofill");
		break;
	case tools::Tool::Type::GRADIENT:
		name = QStringLiteral("gradient");
		break;
	case tools::Tool::Type::ANNOTATION:
		name = QStringLiteral("annotation");
		break;
	case tools::Tool::Type::PICKER:
		name = QStringLiteral("picker");
		break;
	case tools::Tool::Type::LASERPOINTER:
		name = QStringLiteral("laserpointer");
		break;
	case tools::Tool::Type::SELECTION:
		name = QStringLiteral("selection");
		break;
	case tools::Tool::Type::POLYGONSELECTION:
		name = QStringLiteral("polygonselection");
		break;
	case tools::Tool::Type::MAGICWAND:
		name = QStringLiteral("magicwand");
		break;
	case tools::Tool::Type::TRANSFORM:
		name = QStringLiteral("transform");
		break;
	case tools::Tool::Type::PAN:
		name = QStringLiteral("pan");
		break;
	case tools::Tool::Type::ZOOM:
		name = QStringLiteral("zoom");
		break;
	case tools::Tool::Type::ROTATION:
		name = QStringLiteral("rotation");
		break;
	case tools::Tool::Type::INSPECTOR:
		name = QStringLiteral("inspector");
		break;
	case tools::Tool::Type::_LASTTOOL:
		break;
	}

	if(name.isEmpty()) {
		qWarning("Unhandled tool type %d in activity broadcast", type);
	} else {
		sendData(
			QStringLiteral("tool"), QJsonObject({
										{QStringLiteral("type"), name},
									}));
	}
}

void ActivityBroadcast::sendForegroundColor(const QColor &color)
{
	sendData(
		QStringLiteral("foreground"),
		QJsonObject({
			{QStringLiteral("color"), color.name(QColor::HexRgb)},
		}));
}

void ActivityBroadcast::sendPenDown(
	const QPointF &viewPos, const QRectF &viewArea, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation)
{
	sendData(
		QStringLiteral("pendown"),
		QJsonObject({
			{QStringLiteral("view_pos"), pointfToJson(viewPos)},
			{QStringLiteral("view_area"), rectfToJson(viewArea)},
			{QStringLiteral("pressure"), pressure},
			{QStringLiteral("xtilt"), xtilt},
			{QStringLiteral("ytilt"), ytilt},
			{QStringLiteral("rotation"), rotation},
		}));
}

void ActivityBroadcast::sendPenMove(
	const QPointF &viewPos, const QRectF &viewArea, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation)
{
	sendData(
		QStringLiteral("penmove"),
		QJsonObject({
			{QStringLiteral("view_pos"), pointfToJson(viewPos)},
			{QStringLiteral("view_area"), rectfToJson(viewArea)},
			{QStringLiteral("pressure"), pressure},
			{QStringLiteral("xtilt"), xtilt},
			{QStringLiteral("ytilt"), ytilt},
			{QStringLiteral("rotation"), rotation},
		}));
}

void ActivityBroadcast::sendPenHover(
	const QPointF &viewPos, const QRectF &viewArea)
{
	sendData(
		QStringLiteral("penhover"),
		QJsonObject({
			{QStringLiteral("view_pos"), pointfToJson(viewPos)},
			{QStringLiteral("view_area"), rectfToJson(viewArea)},
		}));
}

void ActivityBroadcast::sendPenUp(const QRectF &viewArea)
{
	sendData(
		QStringLiteral("penup"),
		QJsonObject({
			{QStringLiteral("view_area"), rectfToJson(viewArea)},
		}));
}

void ActivityBroadcast::sendData(const QString &type, const QJsonObject &json)
{
	if(m_socket) {
		QByteArray bytes = type.toUtf8() + QByteArrayLiteral(" ") +
						   QJsonDocument(json).toJson(QJsonDocument::Compact);
		m_socket->writeDatagram(
			bytes, QHostAddress::LocalHost, quint16(m_port));
		Q_EMIT activityBroadcasted(bytes);
	}
}

}
