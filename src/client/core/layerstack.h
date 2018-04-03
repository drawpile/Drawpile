/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2018 Calle Laakkonen

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
#ifndef LAYERSTACK_H
#define LAYERSTACK_H

#include <cstdint>

#include <QObject>
#include <QList>
#include <QImage>
#include <QBitArray>

class QDataStream;

#include "annotationmodel.h"

namespace paintcore {

class Layer;
class Tile;
class Savepoint;
struct LayerInfo;

/**
 * \brief A stack of layers.
 */
class LayerStack : public QObject {
	Q_PROPERTY(AnnotationModel* annotations READ annotations CONSTANT)
	Q_OBJECT
public:
	enum ViewMode {
		NORMAL,   // show all layers normally
		SOLO,     // show only the view layer
		ONIONSKIN // show view layer + few layers below it with decreasing opacity
	};

	LayerStack(QObject *parent=nullptr);
	~LayerStack();

	//! Return a copy of this LayerStack
	LayerStack *clone(QObject *newParent=nullptr) const { return new LayerStack(this, newParent); }

	//! Adjust layer stack size
	void resize(int top, int right, int bottom, int left);

	//! Create a new layer
	Layer *createLayer(int id, int source, const QColor &color, bool insert, bool copy, const QString &name);

	//! Delete a layer
	bool deleteLayer(int id);

	//! Merge the layer to the one below it
	void mergeLayerDown(int id);

	//! Re-order the layer stack
	void reorderLayers(const QList<uint16_t> &neworder);

	//! Get the number of layers in the stack
	int layerCount() const { return m_layers.count(); }

	//! Get a layer by its index
	Layer *getLayerByIndex(int index);

	//! Get a read only layer by its index
	const Layer *getLayerByIndex(int index) const;

	//! Get a layer by its ID
	Layer *getLayer(int id);

	//! Get a layer by its ID
	const Layer *getLayer(int id) const;

	//! Get this layer stack's annotations
	const AnnotationModel *annotations() const { return m_annotations; }
	AnnotationModel *annotations() { return m_annotations; }

	//! Get the index of the specified layer
	int indexOf(int id) const;

	//! Get the width of the layer stack
	int width() const { return m_width; }

	//! Get the height of the layer stack
	int height() const { return m_height; }

	//! Get the width and height of the layer stack
	QSize size() const { return QSize(m_width, m_height); }

	//! Paint all changed tiles in the given area
	void paintChangedTiles(const QRect& rect, QPaintDevice *target, bool clean=true);

	//! Return the topmost visible layer with a color at the point
	const Layer *layerAt(int x, int y) const;

	//! Get the merged color value at the point
	QColor colorAt(int x, int y, int dia=0) const;

	//! Return a flattened image of the layer stack
	QImage toFlatImage(bool includeAnnotations) const;

	//! Return a single layer composited with the given background
	QImage flatLayerImage(int layerIdx, bool useBgLayer, const QColor &background) const;

	//! Get a merged tile
	Tile getFlatTile(int x, int y) const;

	//! Mark the tiles under the area dirty
	void markDirty(const QRect &area);

	//! Mark all tiles as dirty and call notifyAreaChanged
	void markDirty();

	//! Mark the tile at the given index as dirty
	void markDirty(int x, int y);

	//! Mark the tile at the given index as dirty
	void markDirty(int index);

	//! Emit areaChanged if anything has been marked as dirty
	void notifyAreaChanged();

	/**
	 * @brief Start a sequence of operations
	 *
	 * notifyAreaChanged is suppressed until endSequence is called.
	 *
	 * Note: you should typically use the LayerStackWriteSequence wrapper class
	 * instead of calling this directly.
	 */
	void beginWriteSequence();

	/**
	 * @brief End of a sequence of operations on this
	 */
	void endWriteSequence();

	//! Is a write sequence underway?
	bool isWriteSequence() const { return m_writeSequence; }

	//! Emit a layer info change notification
	void notifyLayerInfoChange(const Layer *layer);

	//! Create a new savepoint
	Savepoint *makeSavepoint();

	//! Restore layer stack to a previous savepoint
	void restoreSavepoint(const Savepoint *savepoint);

	//! Set layer view mode
	void setViewMode(ViewMode mode);

	ViewMode viewMode() const { return m_viewmode; }

	//! Set the selected layer (used by view modes other than NORMAL)
	void setViewLayer(int id);

	//! Set onionskin view mode parameters
	void setOnionskinMode(int below, int above, bool tint);

	//! Show background layer (bottom-most layer) in special view modes
	void setViewBackgroundLayer(bool usebg);

	//! Clear the entire layer stack
	void reset();

	//! Remove all preview layers (ephemeral sublayers)
	void removePreviews();

	//! Merge all sublayers with the given ID
	void mergeSublayers(int id);

	//! Merge all sublayers with positive IDs
	void mergeAllSublayers();

	/**
	 * @brief Find a layer with a sublayer with the given ID and return its change bounds
	 * @param contextid
	 * @return Layer ID, Change bounds pair
	 */
	QPair<int,QRect> findChangeBounds(int contextid);

signals:
	//! Emitted when the visible layers are edited
	void areaChanged(const QRect &area);

	//! Layer width/height changed
	void resized(int xoffset, int yoffset, const QSize &oldsize);

	//! A layer's info has just changed
	void layerChanged(int idx);

	//! A layer was just added
	void layerCreated(int idx, const LayerInfo &info);

	//! A layer was just deleted
	void layerDeleted(int idx);

	//! All (or at least a lot of) layers have just changed
	void layersChanged(const QList<LayerInfo> &layers);

private:
	LayerStack(const LayerStack *orig, QObject *parent);

	void flattenTile(quint32 *data, int xindex, int yindex) const;

	bool isVisible(int idx) const;
	int layerOpacity(int idx) const;
	quint32 layerTint(int idx) const;

	QList<LayerInfo> layerInfos() const;

	int m_width, m_height;
	int m_xtiles, m_ytiles;
	QList<Layer*> m_layers;
	AnnotationModel *m_annotations;

	QBitArray m_dirtytiles;
	QRect m_dirtyrect;

	ViewMode m_viewmode;
	int m_viewlayeridx;
	int m_onionskinsBelow, m_onionskinsAbove;
	bool m_onionskinTint;
	bool m_viewBackgroundLayer;
	bool m_writeSequence;
};

/// Layer stack savepoint for undo use
class Savepoint {
	friend class LayerStack;
public:
	~Savepoint();

	void toDatastream(QDataStream &out) const;
	static Savepoint *fromDatastream(QDataStream &in, LayerStack *owner);

private:
	Savepoint() {}
	QList<Layer*> layers;
	QList<Annotation> annotations;
	int width, height;
};

/**
 * @brief A RAII wrapper that automatically calls begin and endWriteSequence
 */
class LayerStackWriteSequence {
public:
	explicit LayerStackWriteSequence(LayerStack *layerstack)
		: m_layerstack(layerstack)
	{
		Q_ASSERT(m_layerstack);
		m_layerstack->beginWriteSequence();
	}

	~LayerStackWriteSequence()
	{
		m_layerstack->endWriteSequence();
	}

	LayerStack *operator *() { return m_layerstack; }
	LayerStack *operator ->() { return m_layerstack; }

private:
	LayerStack *m_layerstack;
};

}

#endif

