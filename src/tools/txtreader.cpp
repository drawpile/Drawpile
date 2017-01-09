/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

#include "txtreader.h"
#include "../client/core/blendmodes.h"

#include "../shared/net/annotation.h"
#include "../shared/net/image.h"
#include "../shared/net/layer.h"
#include "../shared/net/meta.h"
#include "../shared/net/pen.h"
#include "../shared/net/undo.h"


using protocol::MessagePtr;

namespace {

class SyntaxError {
public:
	SyntaxError(const QString &e) : error(e) { }

	QString error;
};

int str2int(const QString &str, int base=10) {
	bool ok;
	int i = str.toInt(&ok, base);
	if(!ok)
		throw SyntaxError(QString("'%1'' is not an integer!").arg(str));
	return i;
}

qreal str2real(const QString &str) {
	bool ok;
	qreal i = str.toDouble(&ok);
	if(!ok)
		throw SyntaxError(QString("'%1'' is not a number!").arg(str));
	return i;
}

bool str2bool(const QString &str) {
	if(str.compare("true", Qt::CaseInsensitive)==0)
		return true;
	else if(str.compare("false", Qt::CaseInsensitive)==0)
		return false;
	throw SyntaxError("Expected true/false, got: " + str);
}

int str2ctxid(const QString &str)
{
	int id = str2int(str);
	if(id<1 || id>255)
		throw SyntaxError(QString("ID %1 out of range! (0-255)").arg(id));
	return id;
}

quint32 str2color(const QString &str) {
	if(str.at(0) == '#')
	{
		bool ok;
		uint i = str.mid(1).toUInt(&ok, 16);
		if(!ok)
			throw SyntaxError(QString("'%1'' is not an integer!").arg(str));
		return i;
	} else
		throw SyntaxError("Unsupported color format: " + str);

}

typedef QHash<QString,QString> Params;
typedef QHash<QString,QString>::const_iterator ParamIterator;

Params extractParams(const QString &string)
{
	// get parameters of type "param=value"
	Params params;
	QStringList tokens = string.split(' ', QString::SkipEmptyParts);

	foreach(const QString &token, tokens) {
		int eq = token.indexOf("=");
		if(eq<=0)
			throw SyntaxError("bad parameter definition: " + token);
		params[token.left(eq)] = token.mid(eq+1);
	}
	return params;
}

}

void TextReader::handleResize(const QString &args)
{
	QStringList wh = args.split(' ');
	if(wh.count() != 5)
		throw SyntaxError("Expected context id, top, right, bottom and left");

	_messages.append(MessagePtr(new protocol::CanvasResize(
		str2ctxid(wh[0]),
		str2int(wh[1]),
		str2int(wh[2]),
		str2int(wh[3]),
		str2int(wh[4])
	)));
}

void TextReader::handleNewLayer(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (\\d+) (#[0-9a-fA-F]{8})( copy)?( insert)? (.*)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id, layer id, source id, color, [copy], [insert], and title");

	Layer layer {
		str2int(m.captured(2)),
		m.captured(7),
		1.0,
		paintcore::BlendMode::MODE_NORMAL
	};

	_layer[layer.id] = layer;

	uint8_t flags = 0;
	if(!m.captured(5).isEmpty())
		flags |= protocol::LayerCreate::FLAG_COPY;
	if(!m.captured(6).isEmpty())
		flags |= protocol::LayerCreate::FLAG_INSERT;

	_messages.append(MessagePtr(new protocol::LayerCreate(
		str2int(m.captured(1)),
		str2int(m.captured(2)),
		str2int(m.captured(3)),
		str2color(m.captured(4)),
		flags,
		m.captured(7)
	)));

}

void TextReader::handleLayerAttr(const QString &args)
{
	// extract context ID
	int sep = args.indexOf(' ');
	if(sep<0)
		throw SyntaxError("Expected parameters as well!");

	int ctxid = str2ctxid(args.left(sep));

	// extract layer ID
	int sep2 = args.indexOf(' ', sep+1);
	int id = str2ctxid(args.mid(sep+1, sep2-sep-1));

	Params params = extractParams(args.mid(sep2+1));

	Layer &layer = _layer[id];

	ParamIterator i = params.constBegin();
	while (i!=params.constEnd()) {
		if(i.key() == "opacity")
			layer.opacity = str2real(i.value());
		else if(i.key() == "blend") {
			bool found;
			layer.blend = paintcore::findBlendModeByName(i.value(), &found).id;
			if(!found)
				throw SyntaxError("Unrecognized blending mode: " + i.value());
		} else
			throw SyntaxError("Unrecognized parameter: " + i.key());

		++i;
	}

	_messages.append(MessagePtr(new protocol::LayerAttributes(ctxid, id, layer.opacity*255, layer.blend)));
}

void TextReader::handleRetitleLayer(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (.*)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected id and title");

	Layer &layer = _layer[str2int(m.captured(2))];
	layer.title = m.captured(3);

	_messages.append(MessagePtr(new protocol::LayerRetitle(
		str2ctxid(m.captured(1)),
		layer.id,
		layer.title
	)));
}

void TextReader::handleDeleteLayer(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+)(?: (merge))?");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id, layer id and title");
	_messages.append(MessagePtr(new protocol::LayerDelete(
		str2ctxid(m.captured(1)),
		str2int(m.captured(2)),
		m.captured(3).isEmpty() ? false : true
	)));
}

void TextReader::handleReorderLayers(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.length()<3)
		throw SyntaxError("At least two layers needed!");

	QList<uint16_t> ids;

	foreach(const QString &token, tokens) {
		ids << str2int(token);
	}

	int ctxid = ids.takeFirst();
	_messages.append(MessagePtr(new protocol::LayerOrder(ctxid, ids)));
}


void TextReader::handleDrawingContext(const QString &args)
{
	// extract ID
	int sep = args.indexOf(' ');
	if(sep<0)
		throw SyntaxError("Expected parameters as well!");

	int id = str2ctxid(args.left(sep));

	Params params = extractParams(args.mid(sep+1));

	ToolContext &ctx = _ctx[id];

	ParamIterator i = params.constBegin();
	while (i!=params.constEnd()) {
		if(i.key() == "layer")
			ctx.layer_id = str2ctxid(i.value());
		else if(i.key() == "color")
			ctx.color = str2color(i.value());
		else if(i.key() == "hard") {
			qreal r = str2real(i.value());
			ctx.hard1 = r;
			ctx.hard2 = r;
		} else if(i.key() == "hardh")
			ctx.hard1 = str2real(i.value());
		else if(i.key() == "hardl")
			ctx.hard2 = str2real(i.value());
		else if(i.key() == "size") {
			int val = str2int(i.value());
			ctx.size1 = val;
			ctx.size2 = val;
		} else if(i.key() == "sizeh")
			ctx.size1 = str2int(i.value());
		else if(i.key() == "sizel")
			ctx.size2 = str2int(i.value());
		else if(i.key() == "opacity") {
			qreal r = str2real(i.value());
			ctx.opacity1 =r;
			ctx.opacity2 =r;
		} else if(i.key() == "opacityh")
			ctx.opacity1 = str2real(i.value());
		else if(i.key() == "opacityl")
			ctx.opacity2 = str2real(i.value());
		else if(i.key() == "smudge") {
			qreal s = str2real(i.value());
			ctx.smudge1 = s;
			ctx.smudge2 = s;
		} else if(i.key() == "smudgeh")
			ctx.smudge1 = str2real(i.value());
		else if(i.key() == "smudgel")
			ctx.smudge2 = str2real(i.value());
		else if(i.key() == "resmudge")
			ctx.resmudge = str2int(i.value());
		else if(i.key() == "blend") {
			bool found;
			ctx.blendmode = paintcore::findBlendModeByName(i.value(), &found).id;
			if(!found)
				throw SyntaxError("Unrecognized blending mode: " + i.value());
		} else if(i.key() == "hardedge")
			ctx.subpixel = !str2bool(i.value());
		else if(i.key() == "incremental")
			ctx.incremental = str2bool(i.value());
		else if(i.key() == "spacing")
			ctx.spacing = str2int(i.value());
		else
			throw SyntaxError("Unrecognized parameter: " + i.key());

		++i;
	}

	uint8_t mode = 0;
	mode |= ctx.subpixel ? protocol::TOOL_MODE_SUBPIXEL : 0;
	mode |= ctx.incremental ? protocol::TOOL_MODE_INCREMENTAL : 0;

	_messages.append(MessagePtr(new protocol::ToolChange(
		id,
		ctx.layer_id,
		ctx.blendmode,
		mode,
		ctx.spacing,
		ctx.color,
		ctx.hard1*255,
		ctx.hard2*255,
		ctx.size1,
		ctx.size2,
		ctx.opacity1*255,
		ctx.opacity2*255,
		ctx.smudge1*255,
		ctx.smudge2*255,
		ctx.resmudge
	)));
}

void TextReader::handlePenMove(const QString &args)
{
	int idsep = args.indexOf(' ');
	if(idsep<0)
		throw SyntaxError("Expected ID and point");
	int id = str2ctxid(args.left(idsep));

	protocol::PenPointVector points;
	QStringList pargs = args.mid(idsep+1).split(';', QString::SkipEmptyParts);
	QRegularExpression re("(-?\\d+(?:\\.\\d+)?) (-?\\d+(?:\\.\\d+)?)(?: (\\d(?:\\.\\d+)?))?");
	for(const QString &parg : pargs) {
		QRegularExpressionMatch m = re.match(parg);
		if(!m.hasMatch())
			throw SyntaxError("Invalid point: " + parg);

		points.append(protocol::PenPoint(
			str2real(m.captured(1)) * 4.0,
			str2real(m.captured(2)) * 4.0,
			(m.captured(3).isNull() ? 1.0 : str2real(m.captured(3))) * 0xffff
		));
	}

	_messages.append(MessagePtr(new protocol::PenMove(id, points)));
}

void TextReader::handlePenUp(const QString &args)
{
	int id = str2ctxid(args);
	_messages.append(MessagePtr(new protocol::PenUp(id)));
}

void TextReader::handleInlineImage(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected width and height");

	_inlineImageWidth = str2int(m.captured(1));
	_inlineImageHeight = str2int(m.captured(2));
	_inlineImageData.clear();
}

void TextReader::handlePutImage(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (\\d+) (\\d+) ([\\w-]+) (.+)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id, layer id, x, y, mode and filename");

	int ctxid = str2ctxid(m.captured(1));
	int layer = str2ctxid(m.captured(2));
	int x = str2int(m.captured(3));
	int y = str2int(m.captured(4));
	QString modestr = m.captured(5);

	bool found;
	paintcore::BlendMode::Mode mode = paintcore::findBlendModeByName(modestr, &found).id;

	if(!found)
		throw SyntaxError("Invalid blending mode: " + m.captured(8));

	if(m.captured(6) == "-") {
		// inline image
		QByteArray data = QByteArray::fromBase64(_inlineImageData);

		_messages.append(MessagePtr(new protocol::PutImage(
			ctxid,
			layer,
			mode,
			x,
			y,
			_inlineImageWidth,
			_inlineImageHeight,
			data
		)));

	} else {
		throw SyntaxError("External image references not implemented");
	}
}

void TextReader::handleFillRect(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (\\d+) (\\d+) (\\d+) (\\d+) (#[0-9a-fA-F]{8})(?: ([\\w-]+))?");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id, layer id, x, y, w, h, color and blending mode");

	int ctxid = str2ctxid(m.captured(1));
	int layer = str2ctxid(m.captured(2));
	int x = str2int(m.captured(3));
	int y = str2int(m.captured(4));
	int w = str2int(m.captured(5));
	int h = str2int(m.captured(6));
	quint32 color = str2color(m.captured(7));

	int blend;
	if(m.captured(8).isEmpty())
		blend = 255;
	else {
		bool found;
		blend = paintcore::findBlendModeByName(m.captured(8), &found).id;

		if(!found)
			throw SyntaxError("Invalid blending mode: " + m.captured(8));
	}

	_messages.append(MessagePtr(new protocol::FillRect(ctxid, layer, blend, x, y, w, h, color)));

}

void TextReader::handleUndoPoint(const QString &args)
{
	int ctxid = str2ctxid(args);

	_messages.append(MessagePtr(new protocol::UndoPoint(ctxid)));
}

void TextReader::handleUndo(const QString &args)
{
	QRegularExpression re("(\\d+) (-?\\d+)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id and undo count");

	int ctxid = str2ctxid(m.captured(1));
	int count = str2int(m.captured(2));
	if(count==0)
		throw SyntaxError("zero undo is not allowed");

	_messages.append(MessagePtr(new protocol::Undo(ctxid, 0, count)));
}

void TextReader::handleAddAnnotation(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.count() != 6)
		throw SyntaxError("Expected context id, annotation id, position and size");

	int ctxid = str2ctxid(tokens[0]);
	int annid = str2ctxid(tokens[1]);
	int x = str2int(tokens[2]);
	int y = str2int(tokens[3]);
	int w = str2int(tokens[4]);
	int h = str2int(tokens[5]);

	_messages.append(MessagePtr(new protocol::AnnotationCreate(ctxid, annid, x, y, w, h)));
}

void TextReader::handleReshapeAnnotation(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.count() != 6)
		throw SyntaxError("Expected context id, annotation id, position and size");

	int ctxid = str2ctxid(tokens[0]);
	int annid = str2ctxid(tokens[1]);
	int x = str2int(tokens[2]);
	int y = str2int(tokens[3]);
	int w = str2int(tokens[4]);
	int h = str2int(tokens[5]);

	_messages.append(MessagePtr(new protocol::AnnotationReshape(ctxid, annid, x, y, w, h)));
}

void TextReader::handleDeleteAnnotation(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.count() != 2)
		throw SyntaxError("Expected context id and annotation id");

	int ctx = str2ctxid(tokens[0]);
	int id = str2ctxid(tokens[1]);

	_messages.append(MessagePtr(new protocol::AnnotationDelete(ctx, id)));
}

void TextReader::handlEditAnnotation(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.count() != 3)
		throw SyntaxError("Expected context id, annotation id and background color");

	_edit_a_ctx = str2ctxid(tokens[0]);
	_edit_a_id = str2ctxid(tokens[1]);
	_edit_a_color = str2color((tokens[2]));
	_edit_a_text = "";
}

void TextReader::editAnnotationDone()
{
	_messages.append(MessagePtr(new protocol::AnnotationEdit(_edit_a_ctx, _edit_a_id, _edit_a_color, _edit_a_text)));
}

bool TextReader::load()
{
	QFile file(_filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		_error = file.errorString();
		return false;
	}
	QTextStream text(&file);

	QString line;
	enum {NORMAL, ANNOTATION, INLINE_IMAGE} mode = NORMAL;
	int linenumber=0;
	while(!(line=text.readLine()).isNull()) {
		++linenumber;
		if(mode == ANNOTATION) {
			if(line == "endannotate") {
				mode = NORMAL;
				editAnnotationDone();
			} else {
				_edit_a_text += line;
				_edit_a_text += "\n";
			}
			continue;
		} else if(mode == INLINE_IMAGE) {
			if(line == "==end==") {
				mode = NORMAL;
			} else {
				_inlineImageData += line.toLatin1();
			}
			continue;
		}

		line = line.trimmed();
		if(line.length()==0 || line.at(0) == '#')
			continue;

		int sep = line.indexOf(' ');
		QString cmd, args;
		if(sep<0) {
			cmd = line;
		} else {
			cmd = line.left(sep);
			args = line.mid(sep+1);
		}

		try {
			if(cmd=="resize")
				handleResize(args);
			else if(cmd=="newlayer")
				handleNewLayer(args);
			else if(cmd=="layerattr")
				handleLayerAttr(args);
			else if(cmd=="retitlelayer")
				handleRetitleLayer(args);
			else if(cmd=="deletelayer")
				handleDeleteLayer(args);
			else if(cmd=="reorderlayers")
				handleReorderLayers(args);
			else if(cmd=="ctx")
				handleDrawingContext(args);
			else if(cmd=="move")
				handlePenMove(args);
			else if(cmd=="penup")
				handlePenUp(args);
			else if(cmd=="inlineimage") {
				handleInlineImage(args);
				mode = INLINE_IMAGE;
			} else if(cmd=="putimage")
				handlePutImage(args);
			else if(cmd=="fillrect")
				handleFillRect(args);
			else if(cmd=="undopoint")
				handleUndoPoint(args);
			else if(cmd=="undo")
				handleUndo(args);
			else if(cmd=="addannotation")
				handleAddAnnotation(args);
			else if(cmd=="reshapeannotation")
				handleReshapeAnnotation(args);
			else if(cmd=="deleteannotation")
				handleDeleteAnnotation((args));
			else if(cmd=="annotate") {
				handlEditAnnotation(args);
				mode = ANNOTATION;
			} else {
				_error = QString("Unrecognized command on line %1: %2").arg(linenumber).arg(cmd);
				return false;
			}
		} catch(const SyntaxError &e) {
			_error = QString("Error on line %1: %2").arg(linenumber).arg(e.error);
			return false;
		}
	}

	return true;
}

