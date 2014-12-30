/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#include <QFileInfo>
#include <QDir>

#include "textloader.h"
#include "core/rasterop.h"
#include "net/utils.h"

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

void TextCommandLoader::handleResize(const QString &args)
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

void TextCommandLoader::handleNewLayer(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (#[0-9a-fA-F]{8}) (.*)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id, layer id, color and title");

	net::LayerListItem layer(str2int(m.captured(2)), m.captured(4));
	_layer[layer.id] = layer;

	_messages.append(MessagePtr(new protocol::LayerCreate(
		str2int(m.captured(1)),
		str2int(m.captured(2)),
		str2color(m.captured(3)),
		m.captured(4)
	)));
}

void TextCommandLoader::handleLayerAttr(const QString &args)
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

	net::LayerListItem &layer = _layer[id];

	ParamIterator i = params.constBegin();
	while (i!=params.constEnd()) {
		if(i.key() == "opacity")
			layer.opacity = str2real(i.value());
		else if(i.key() == "blend") {
			int mode = paintcore::blendModeSvg(i.value());
			if(mode<0)
				throw SyntaxError("Unrecognized blending mode: " + i.value());
			layer.blend = mode;
		} else
			throw SyntaxError("Unrecognized parameter: " + i.key());

		++i;
	}

	_messages.append(MessagePtr(new protocol::LayerAttributes(ctxid, id, layer.opacity*255, layer.blend)));
}

void TextCommandLoader::handleRetitleLayer(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (.*)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected id and title");

	net::LayerListItem &layer = _layer[str2int(m.captured(2))];
	layer.title = m.captured(3);

	_messages.append(MessagePtr(new protocol::LayerRetitle(
		str2ctxid(m.captured(1)),
		layer.id,
		layer.title
	)));
}

void TextCommandLoader::handleDeleteLayer(const QString &args)
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

void TextCommandLoader::handleReorderLayers(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.length()<3)
		throw SyntaxError("At least two layers needed!");

	QList<uint8_t> ids;

	foreach(const QString &token, tokens) {
		ids << str2int(token);
	}

	int ctxid = ids.takeFirst();
	_messages.append(MessagePtr(new protocol::LayerOrder(ctxid, ids)));
}


void TextCommandLoader::handleDrawingContext(const QString &args)
{
	// extract ID
	int sep = args.indexOf(' ');
	if(sep<0)
		throw SyntaxError("Expected parameters as well!");

	int id = str2ctxid(args.left(sep));

	Params params = extractParams(args.mid(sep+1));

	drawingboard::ToolContext &ctx = _ctx[id];

	ParamIterator i = params.constBegin();
	while (i!=params.constEnd()) {
		if(i.key() == "layer")
			ctx.layer_id = str2ctxid(i.value());
		else if(i.key() == "color") {
			QColor c = str2color(i.value());
			ctx.brush.setColor(c);
			ctx.brush.setColor2(c);
		} else if(i.key() == "colorh")
			ctx.brush.setColor(str2color(i.value()));
		else if(i.key() == "colorl")
			ctx.brush.setColor2(str2color(i.value()));
		else if(i.key() == "hard") {
			qreal r = str2real(i.value());
			ctx.brush.setHardness(r);
			ctx.brush.setHardness2(r);
		} else if(i.key() == "hardh")
			ctx.brush.setHardness(str2real(i.value()));
		else if(i.key() == "hardl")
			ctx.brush.setHardness2(str2real(i.value()));
		else if(i.key() == "size") {
			int val = qMax(1, str2int(i.value())) * 2;
			ctx.brush.setSize(val);
			ctx.brush.setSize2(val);
		} else if(i.key() == "sizeh")
			ctx.brush.setSize(qMax(1, str2int(i.value())));
		else if(i.key() == "sizel")
			ctx.brush.setSize2(qMax(1, str2int(i.value())));
		else if(i.key() == "opacity") {
			qreal r = str2real(i.value());
			ctx.brush.setOpacity(r);
			ctx.brush.setOpacity2(r);
		} else if(i.key() == "opacityh")
			ctx.brush.setOpacity(str2real(i.value()));
		else if(i.key() == "opacityl")
			ctx.brush.setOpacity2(str2real(i.value()));
		else if(i.key() == "smudge") {
			qreal s = str2real(i.value());
			ctx.brush.setSmudge(s);
			ctx.brush.setSmudge2(s);
		} else if(i.key() == "smudgeh")
			ctx.brush.setSmudge(str2real(i.value()));
		else if(i.key() == "smudgel")
			ctx.brush.setSmudge2(str2real(i.value()));
		else if(i.key() == "resmudge")
			ctx.brush.setResmudge(str2int(i.value()));
		else if(i.key() == "blend") {
			int mode = paintcore::blendModeSvg(i.value());
			if(mode<0)
				throw SyntaxError("Unrecognized blending mode: " + i.value());
			ctx.brush.setBlendingMode(mode);
		} else if(i.key() == "hardedge")
			ctx.brush.setSubpixel(!str2bool(i.value()));
		else if(i.key() == "incremental")
			ctx.brush.setIncremental(str2bool(i.value()));
		else if(i.key() == "spacing")
			ctx.brush.setSpacing(str2int(i.value()));
		else
			throw SyntaxError("Unrecognized parameter: " + i.key());

		++i;
	}

	_messages.append(MessagePtr(net::brushToToolChange(id, ctx.layer_id, ctx.brush)));
}

void TextCommandLoader::handlePenMove(const QString &args)
{
	int idsep = args.indexOf(' ');
	if(idsep<0)
		throw SyntaxError("Expected ID and point");
	int id = str2ctxid(args.left(idsep));

	paintcore::PointVector points;
	QStringList pargs = args.mid(idsep+1).split(';', QString::SkipEmptyParts);
	QRegularExpression re("(\\d+(?:\\.\\d+)?) (\\d+(?:\\.\\d+)?)(?: (\\d(?:\\.\\d+)?))?");
	foreach(const QString &parg, pargs) {
		QRegularExpressionMatch m = re.match(parg);
		if(!m.hasMatch())
			throw SyntaxError("Invalid point: " + parg);

		points.append(paintcore::Point(
			QPointF(
				str2real(m.captured(1)),
				str2real(m.captured(2))
			),
			m.captured(3).isNull() ? 1.0 : str2real(m.captured(3))
		));
	}

	_messages.append(MessagePtr(new protocol::PenMove(id, net::pointsToProtocol(points))));
}

void TextCommandLoader::handlePenUp(const QString &args)
{
	int id = str2ctxid(args);
	_messages.append(MessagePtr(new protocol::PenUp(id)));
}

void TextCommandLoader::handleInlineImage(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected width and height");

	_inlineImageWidth = str2int(m.captured(1));
	_inlineImageHeight = str2int(m.captured(2));
	_inlineImageData.clear();
}

void TextCommandLoader::handlePutImage(const QString &args)
{
	QRegularExpression re("(\\d+) (\\d+) (\\d+) (\\d+)(?: (blend))? (.+)");
	QRegularExpressionMatch m = re.match(args);
	if(!m.hasMatch())
		throw SyntaxError("Expected context id, layer id, x, y and filename");

	int ctxid = str2ctxid(m.captured(1));
	int layer = str2ctxid(m.captured(2));
	int x = str2int(m.captured(3));
	int y = str2int(m.captured(4));
	bool blend = !m.captured(5).isEmpty();

	if(m.captured(6) == "-") {
		// inline image
		QByteArray data = QByteArray::fromBase64(_inlineImageData);

		_messages.append(MessagePtr(new protocol::PutImage(
			ctxid,
			layer,
			blend ? protocol::PutImage::MODE_BLEND : 0,
			x,
			y,
			_inlineImageWidth,
			_inlineImageHeight,
			data
		)));

	} else {
		// external file reference
		QFileInfo filename(QFileInfo(_filename).dir(), m.captured(6));

		QImage image(filename.absoluteFilePath());
		_messages.append(net::putQImage(ctxid, layer, x, y, image, blend));
	}


}

void TextCommandLoader::handleFillRect(const QString &args)
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
	else
		blend = paintcore::blendModeSvg(m.captured(8));

	if(blend<0)
		throw SyntaxError("Invalid blending mode: " + m.captured(8));

	_messages.append(MessagePtr(new protocol::FillRect(ctxid, layer, blend, x, y, w, h, color)));

}

void TextCommandLoader::handleUndoPoint(const QString &args)
{
	int ctxid = str2ctxid(args);

	_messages.append(MessagePtr(new protocol::UndoPoint(ctxid)));
}

void TextCommandLoader::handleUndo(const QString &args)
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

void TextCommandLoader::handleAddAnnotation(const QString &args)
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

void TextCommandLoader::handleReshapeAnnotation(const QString &args)
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

void TextCommandLoader::handleDeleteAnnotation(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.count() != 2)
		throw SyntaxError("Expected context id and annotation id");

	int ctx = str2ctxid(tokens[0]);
	int id = str2ctxid(tokens[1]);

	_messages.append(MessagePtr(new protocol::AnnotationDelete(ctx, id)));
}

void TextCommandLoader::handlEditAnnotation(const QString &args)
{
	QStringList tokens = args.split(' ', QString::SkipEmptyParts);
	if(tokens.count() != 3)
		throw SyntaxError("Expected context id, annotation id and background color");

	_edit_a_ctx = str2ctxid(tokens[0]);
	_edit_a_id = str2ctxid(tokens[1]);
	_edit_a_color = str2color((tokens[2]));
	_edit_a_text = "";
}

void TextCommandLoader::editAnnotationDone()
{
	_messages.append(MessagePtr(new protocol::AnnotationEdit(_edit_a_ctx, _edit_a_id, _edit_a_color, _edit_a_text)));
}

bool TextCommandLoader::load()
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

