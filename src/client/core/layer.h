/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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
#ifndef PAINTCORE_LAYER_H
#define PAINTCORE_LAYER_H

#include "tile.h"

#include <QVector>
#include <QColor>
#include <QRect>

class QImage;
class QSize;
class QDataStream;

namespace paintcore {

class Brush;
struct BrushStamp;
class Point;
class LayerStack;
struct StrokeState;

/**
 * @brief The non-pixeldata part of the layer
 */
struct LayerInfo {
	// Identification
	// Note: In the protocol, layer IDs are 16bit, but for internal (ephemeral) layers,
	// we use IDs outside that range.
	int id;
	QString title;

	// Rendering controls
	uchar opacity = 255;
	bool hidden = false;
	bool censored = false;
	bool fixed = false;
	BlendMode::Mode blend = BlendMode::MODE_NORMAL;

	LayerInfo() = default;
	LayerInfo(int id, const QString &title) : id(id), title(title) { }
};

/**
 * @brief A drawing layer/tile manage
 * 
 * A layer is made up of multiple tiles.
 * Although images of arbitrary size can be created, the true layer size is
 * always a multiple of Tile::SIZE.
 *
 * Layer editing functions are provided via the EditableLayer wrapper class.
 * However, you should typically not instantiate this class yourself. Instead,
 * use the LayerStackWriteSequence class.
 *
 * Exceptions: when you're dealing with free layers (not part of any LayerStack)
 */
class Layer {
	friend class EditableLayer;
public:
	//! Construct a layer filled with solid color
	Layer(int id, const QString& title, const QColor& color, const QSize& size);

	Layer(const QVector<Tile> &tiles, const QSize &size, const LayerInfo &info, const QList<Layer*> sublayers);

	//! Construct a copy of this layer
	Layer(const Layer &layer);	

	Layer &operator=(const Layer &other) = delete;

	~Layer();

	//! Get the layer width in pixels
	int width() const { return m_width; }

	//! Get the layer height in pixels
	int height() const { return m_height; }

	//! Get the layer ID
	int id() const { return m_info.id; }

	//! Get the layer name
	const QString& title() const { return m_info.title; }

	//! Get the layer as an image
	QImage toImage() const;

	//! Get the layer as an image with excess transparency cropped away
	QImage toCroppedImage(int *xOffset, int *yOffset) const;

	//! Get the color at the specified coordinates
	QColor colorAt(int x, int y, int dia=0) const;

	//! Get the raw pixel value at the specified coordinates
	QRgb pixelAt(int x, int y) const;

	//! Get layer opacity
	int opacity() const { return m_info.opacity; }

	//! Get the effective layer opacity (0 if hidden)
	int effectiveOpacity() const { return isHidden() ? 0 : m_info.opacity; }

	/**
	 * @brief Get the layer blending mode
	 * @return blending mode number
	 */
	BlendMode::Mode blendmode() const { return m_info.blend; }

	/**
	 * @brief Is this layer hidden?
	 * Hiding a layer is slightly different than setting its opacity
	 * to zero, although the end result is the same. The hidden status
	 * is purely local: setting it will not hide the layer for other
	 * users.
	 */
	bool isHidden() const { return m_info.hidden; }

	//! Is this layer flagged for censoring
	bool isCensored() const { return m_info.censored; }

	//! Is this a fixed background/foreground layer
	bool isFixed() const { return m_info.fixed; }

	/**
	 * @brief Get a sublayer
	 *
	 * Sublayers are used for indirect drawing and previews.
	 * Positive IDs should correspond to context IDs: they are used for indirect
	 * painting.
	 * Negative IDs are local preview layers.
	 *
	 * ID 0 should not be used.
	 *
	 * The blendmode and opacity are set only when the layer is created.
	 *
	 * @param id
	 * @param blendmode
	 * @param opacity
	 * @return
	 */
	Layer *getSubLayer(int id, BlendMode::Mode blendmode, uchar opacity);

	/**
	 * @brief Get a sublayer, but only if it is visible
	 * @param id
	 * @return sublayer or nullptr
	 */
	const Layer *getVisibleSublayer(int id) const;

	//! Get a tile
	const Tile &tile(int x, int y) const {
		Q_ASSERT(x>=0 && x<m_xtiles);
		Q_ASSERT(y>=0 && y<m_ytiles);
		return m_tiles[y*m_xtiles+x];
	}

	//! Get a tile
	const Tile &tile(int index) const { Q_ASSERT(index>=0 && index<m_xtiles*m_ytiles); return m_tiles[index]; }

	//! Get the sublayers
	const QList<Layer*> &sublayers() const { return m_sublayers; }

	/**
	 * @brief Is this layer visible
	 * A layer is visible when its opacity is greater than zero AND
	 * it is not explicitly hidden.
	 * @return true if layer is visible
	 */
	bool isVisible() const { return m_info.opacity > 0 && !m_info.hidden; }

	/**
	 * @brief Get the non-pixeldata related properties
	 */
	const LayerInfo &info() const { return m_info; }

	//! Get this layer's tile vector
	const QVector<Tile> tiles() const { return m_tiles; }

	/**
	 * @brief Get the layer's change bounds
	 */
	QRect changeBounds() const { return m_changeBounds; }

	/**
	 * @brief Get the change bounds of a sublayer
	 * @param contextId sublayer ID
	 */
	QRect changeBounds(int contextId) const {
		for(const Layer *l : m_sublayers) {
			if(l->id() == contextId && l->isVisible())
				return l->changeBounds();
		}
		return QRect();
	}

	//! Optimize layer memory usage
	void optimize();

private:
	//! Construct a sublayer
	Layer(int id, const QSize& size);
	Layer padImageToTileBoundary(int leftpad, int toppad, const QImage &original, BlendMode::Mode mode, int contextId) const;
	QColor getDabColor(const BrushStamp &stamp) const;

	Tile &rtile(int x, int y) {
		Q_ASSERT(x>=0 && x<m_xtiles);
		Q_ASSERT(y>=0 && y<m_ytiles);
		return m_tiles[y*m_xtiles+x];
	}


	LayerInfo m_info;
	QRect m_changeBounds;

	QVector<Tile> m_tiles;
	QList<Layer*> m_sublayers;

	int m_width;
	int m_height;
	int m_xtiles;
	int m_ytiles;
};

/**
 * A wrapper for Layer that provides editing functions.
 *
 * Unless you're working with free layers (layers not belonging to any
 * LayerStack), you should use the stack's getEditableLayer function to
 * get an editable reference to a layer.
 */
class EditableLayer {
public:
	EditableLayer() : d(nullptr), owner(nullptr) { }
	EditableLayer(Layer *layer, LayerStack *owner, int contextId)
		: d(layer), owner(owner), contextId(contextId)
	{ Q_ASSERT(layer); }

	//! Is this a null layer?
	bool isNull() const { return !d;}

	//! Get the underlying layer (read-only)
	const Layer *layer() const { return d; }

	//! Change layer ID
	void setId(int id) { d->m_info.id = id; }

	//! Set the layer name
	void setTitle(const QString& title) { d->m_info.title = title; }

	//! Set layer opacity
	void setOpacity(int opacity);

	//! Set layer blending mode
	void setBlend(BlendMode::Mode blend);

	//! Hide this layer
	void setHidden(bool hide);

	//! Censor this layer
	void setCensored(bool censor);

	//! Make this a fixed layer
	void setFixed(bool fixed);

	//! Empty this layer
	void makeBlank();

	//! Draw an image onto the layer
	void putImage(int x, int y, QImage image, BlendMode::Mode mode);

	//! Set a tile
	void putTile(int col, int row, int repeat, const Tile &tile, int sublayer=0);

	//! Dab a brush
	void putBrushStamp(const BrushStamp &bs, const QColor &color, BlendMode::Mode blendmode);

	//! Fill a rectangle
	void fillRect(const QRect &rect, const QColor &color, BlendMode::Mode blendmode);

	//! Get a reference to a tile
	Tile &rtile(int x, int y) { Q_ASSERT(d); return d->rtile(x, y); }

	Tile &rtile(int index) { return d->m_tiles[index]; }

	//! Merge a sublayer with this layer
	void mergeSublayer(int id);

	//! Merge all sublayers with positive IDs
	void mergeAllSublayers();

	//! Remove a sublayer
	void removeSublayer(int id);

	//! Remove all preview (ephemeral) sublayers
	void removePreviews();

	//! Merge a layer
	void merge(const Layer *layer);

	/**
	 * @brief Add the given rectangle to this layer's change bounds
	 *
	 * The change bounds is a cached bounding rectangle of changes made
	 * to a private layer.
	 *
	 * @param b
	 */
	void updateChangeBounds(const QRect &b) { Q_ASSERT(d); d->m_changeBounds |= b; }

	EditableLayer getEditableSubLayer(int id, BlendMode::Mode blendmode, uchar opacity) { return EditableLayer(d->getSubLayer(id, blendmode, opacity), owner, contextId); }

    const Layer *operator ->() const { return d; }

	/**
	 * @brief Adjust layer size
	 *
	 * Note: Unless you're working with a free layer, you should not call
	 * this yourself. Instead, use EditableLayerStack's resize function.
	 */
	void resize(int top, int right, int bottom, int left);

	void markOpaqueDirty(bool forceVisible=false);

private:
	Layer *d;
	LayerStack *owner;
	int contextId;
};

}

Q_DECLARE_TYPEINFO(paintcore::LayerInfo, Q_MOVABLE_TYPE);

#endif

