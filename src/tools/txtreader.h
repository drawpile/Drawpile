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
#ifndef DP_TEXTREADER_H
#define DP_TEXTREADER_H

#include "../shared/net/message.h"
#include "../shared/net/pen.h"

#include <QHash>

struct ToolContext {
	int layer_id;

	int size1, size2;
	qreal hard1, hard2;
	qreal opacity1, opacity2;
	uint32_t color1, color2;
	qreal smudge1, smudge2;
	int resmudge;
	int spacing;
	bool subpixel;
	bool incremental;
	int blendmode;

	ToolContext() :
		layer_id(0),
		size1(1), size2(1),
		hard1(1), hard2(1),
		opacity1(1), opacity2(1),
		color1(0xff000000), color2(0xff000000),
		smudge1(0), smudge2(0),
		resmudge(1),
		spacing(25),
		subpixel(false),
		incremental(true),
		blendmode(1)
	{ }
};

struct Layer {
	int id;
	QString title;
	qreal opacity;
	int blend;
};

/**
 * @brief Parse a text serialization of a recording
 *
 * This is based on TextLoader from the client source code, with the limitation
 * that it cannot load images from files, since this is used in a command line
 * tool that doesn't link to QtQui.
 */
class TextReader {
public:
	TextReader(const QString &filename) : _filename(filename) {}

	bool load();

	QList<protocol::MessagePtr> messages() { return _messages; }
	QString filename() const { return _filename; }
	QString errorMessage() const { return _error; }

private:

	void handleResize(const QString &args);
	void handleNewLayer(const QString &args);
	void handleCopyLayer(const QString &args);
	void handleLayerAttr(const QString &args);
	void handleRetitleLayer(const QString &args);
	void handleDeleteLayer(const QString &args);
	void handleReorderLayers(const QString &args);

	void handleDrawingContext(const QString &args);
	void handlePenMove(const QString &args);
	void handlePenUp(const QString &args);
	void handleInlineImage(const QString &args);
	void handlePutImage(const QString &args);
	void handleFillRect(const QString &args);

	void handleUndoPoint(const QString &args);
	void handleUndo(const QString &args);

	void handleAddAnnotation(const QString &args);
	void handleReshapeAnnotation(const QString &args);
	void handlEditAnnotation(const QString &args);
	void editAnnotationDone();
	void handleDeleteAnnotation(const QString &args);

	QString _filename;
	QString _error;
	QList<protocol::MessagePtr> _messages;
	QHash<int, ToolContext> _ctx;
	QHash<int, Layer> _layer;

	// Annotation edit buffer
	int _edit_a_ctx;
	int _edit_a_id;
	quint32 _edit_a_color;
	QString _edit_a_text;

	// Inline image buffer
	int _inlineImageWidth, _inlineImageHeight;
	QByteArray _inlineImageData;
};

#endif
