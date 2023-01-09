/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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

#include "textmode.h"

#include "meta.h"
#include "meta2.h"
#include "brushes.h"
#include "annotation.h"
#include "layer.h"
#include "image.h"
#include "recording.h"
#include "undo.h"

namespace protocol {
namespace text {

Parser::Result Parser::parseLine(const QString &line)
{
	switch(m_state) {
	case ExpectCommand: {
		if(line.isEmpty() || line.at(0) == '#')
			return Result { Result::Skip, nullptr };

		if(line.at(0) == '!') {
			// Metadata line
			int i = line.indexOf('=');
			if(i>1) {
				QString key = line.mid(1, i-1);
				QString value = line.mid(i+1);
				m_metadata[key] = value;
			}
			return Result { Result::Skip, nullptr };
		}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
		QStringList tokens = line.split(' ', QString::SkipEmptyParts);
#else
		QStringList tokens = line.split(' ', Qt::SkipEmptyParts);
#endif
		if(tokens.length() < 2) {
			m_error = "Expected at least two tokens";
			return Result { Result::Error, nullptr };
		}

		// Get context ID
		{
			bool ok;
			int ctxId = tokens[0].toInt(&ok);
			if(!ok || ctxId<0 || ctxId>255) {
				m_error = "Invalid context id: " + tokens[0];
				return Result { Result::Error, nullptr };
			}
			m_ctx = ctxId;
		}

		// Get message name
		m_cmd = tokens[1];
		m_kwargs = Kwargs();
		m_dabs = QStringList();

		// Check if this is a multiline message
		bool multiline = false;
		if(tokens.size() > 2 && tokens.last() == "{") {
			tokens.removeLast();
			multiline = true;
		}

		// Extract named arguments
		for(int i=2;i<tokens.size();++i) {
			int j = tokens[i].indexOf('=');
			if(j<0) {
				m_error = "No value in keyword argument: " + tokens[i];
				return Result { Result::Error, nullptr };
			}
			m_kwargs[tokens[i].left(j)] = tokens[i].mid(j+1);
		}

		if(multiline) {
			if(m_cmd.endsWith("dabs"))
				m_state = ExpectDab;
			else
				m_state = ExpectKwargLine;

			return Result {Result::NeedMore, nullptr };
		} else
			break;
	}

	case ExpectKwargLine: {
		// Extract named argument
		if(line=="}")
			break;

		const int i = line.indexOf('=');
		if(i<0) {
			m_error = "Invalid named argument: " + line;
			return Result { Result::Error, nullptr };
		}
		const QString name = line.left(i);
		const QString value = line.mid(i+1);
		if(m_kwargs.contains(name)) {
			m_kwargs[name] += "\n";
			m_kwargs[name] += value;
		} else
			m_kwargs[name] = value;

		return { Result::NeedMore, nullptr };
	}

	case ExpectDab: {
		// Extract dab points
		if(line=="}")
			break;
		m_dabs << line.split(' ');
		return { Result::NeedMore, nullptr };
	}
	}

	// Message finished!
	m_state = ExpectCommand;

	Message *msg=nullptr;

#define FROMTEXT(name, Cls) if(m_cmd==name) msg = Cls::fromText(uint8_t(m_ctx), m_kwargs)
	if(m_cmd=="classicdabs") msg = DrawDabsClassic::fromText(m_ctx, m_kwargs, m_dabs);
	else if(m_cmd=="pixeldabs") msg = DrawDabsPixel::fromText(DabShape::Round, m_ctx, m_kwargs, m_dabs);
	else if(m_cmd=="squarepixeldabs") msg = DrawDabsPixel::fromText(DabShape::Square, m_ctx, m_kwargs, m_dabs);
	else FROMTEXT("join", UserJoin);
	else FROMTEXT("leave", UserLeave);
	else FROMTEXT("owner", SessionOwner);
	else FROMTEXT("chat", Chat);
	else FROMTEXT("pm", PrivateChat);
	else FROMTEXT("trusted", TrustedUsers);
	else FROMTEXT("interval", Interval);
	else FROMTEXT("marker", Marker);
	else FROMTEXT("laser", LaserTrail);
	else FROMTEXT("movepointer", MovePointer);
	else FROMTEXT("useracl", UserACL);
	else FROMTEXT("layeracl", LayerACL);
	else FROMTEXT("featureaccess", FeatureAccessLevels);
	else FROMTEXT("defaultlayer", DefaultLayer);
	else FROMTEXT("resize", CanvasResize);
	else FROMTEXT("background", CanvasBackground);
	else FROMTEXT("newlayer", LayerCreate);
	else FROMTEXT("layerattr", LayerAttributes);
	else FROMTEXT("retitlelayer", LayerRetitle);
	else FROMTEXT("layerorder", LayerOrder);
	else FROMTEXT("deletelayer", LayerDelete);
	else FROMTEXT("layervisibility", LayerVisibility);
	else FROMTEXT("putimage", PutImage);
	else FROMTEXT("fillrect", FillRect);
	else FROMTEXT("penup", PenUp);
	else FROMTEXT("newannotation", AnnotationCreate);
	else FROMTEXT("reshapeannotation", AnnotationReshape);
	else FROMTEXT("editannotation", AnnotationEdit);
	else FROMTEXT("deleteannotation", AnnotationDelete);
	else FROMTEXT("moveregion", MoveRegion);
	else FROMTEXT("undopoint", UndoPoint);
	else FROMTEXT("puttile", PutTile);
	else FROMTEXT("softreset", SoftResetPoint);
	else if(m_cmd=="undo") msg = Undo::fromText(m_ctx, m_kwargs, false);
	else if(m_cmd=="redo") msg = Undo::fromText(m_ctx, m_kwargs, true);
	else {
		m_error = "Unknown message type: " + m_cmd;
		return { Result::Error, nullptr };
	}
#undef FROMTEXT

	if(msg) {
		return { Result::Ok, NullableMessageRef(msg) };

	} else {
		m_error = "Couldn't parse " + m_cmd + " message.";
		return { Result::Error, nullptr };
	}
}

QString idListString(QList<uint8_t> ids)
{
	QStringList sl;
	sl.reserve(ids.length());
	for(const uint8_t i : ids)
		sl << QString::number(i);
	return sl.join(',');
}

QString idListString(QList<uint16_t> ids)
{
	QStringList sl;
	sl.reserve(ids.length());
	for(const uint16_t i : ids)
		sl << idString(i);
	return sl.join(',');
}

QString rgbString(quint32 color)
{
	return QStringLiteral("#%1").arg(color & 0x00ffffff, 6, 16, QLatin1Char('0'));
}

QString argbString(quint32 color)
{
	if((color & 0xff000000) == 0xff000000)
		return rgbString(color);
	else
		return QStringLiteral("#%1").arg(color, 8, 16, QLatin1Char('0'));
}

quint32 parseColor(const QString &color)
{
	if((color.length() == 7 || color.length() == 9) && color.at(0) == '#') {
		bool ok;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		quint32 c = QStringView(color).sliced(1).toUInt(&ok, 16);
#else
		quint32 c = color.midRef(1).toUInt(&ok, 16);
#endif
		if(ok)
			return color.length() == 7 ? 0xff000000 | c : c;
	}
	return 0;
}

QList<uint8_t> parseIdListString8(const QString &ids)
{
	QList<uint8_t> list;
	for(const QString &idstr : ids.split(',')) {
		bool ok;
		int id = idstr.toInt(&ok);
		if(ok && id>=0 && id<256)
			list << id;
	}
	return list;
}

uint16_t parseIdString16(const QString &idstr, bool *allOk)
{
	bool ok;
	int id;
	if(idstr.startsWith("0x")) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		id = QStringView(idstr).sliced(2).toInt(&ok, 16);
#else
		id = idstr.midRef(2).toInt(&ok, 16);
#endif
	} else
		id = idstr.toInt(&ok);

	if(ok && id>=0 && id<=0xffff) {
		if(allOk)
			*allOk = true;
		return id;
	} else {
		if(allOk)
			*allOk = false;
		return 0;
	}
}

QList<uint16_t> parseIdListString16(const QString &ids)
{
	QList<uint16_t> list;
	for(const QString &idstr : ids.split(',')) {
		bool ok;
		uint16_t id = parseIdString16(idstr, &ok);
		if(ok)
			list << id;
	}
	return list;
}

}
}

