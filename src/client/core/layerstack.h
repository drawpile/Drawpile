/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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
#include <QMutex>

class QDataStream;

namespace paintcore {

class Layer;
class Tile;
class Savepoint;
struct LayerInfo;

/**
 * \brief A stack of layers.
 */
class LayerStack : public QObject {
	Q_OBJECT
public:
	enum ViewMode {
		NORMAL,   // show all layers normally
		SOLO,     // show only the view layer
		ONIONSKIN // show view layer + few layers below it with decreasing opacity
	};

	LayerStack(QObject *parent=0);
	~LayerStack();

	/**
	 * @brief Acquire a mutex
	 *
	 * This also blocks the emission of areaChanged and resized
	 * until unlock() is called.
	 *
	 * Note. Rather than calling this directly, it's usually easier to use
	 * the Locker class.
	 * @return true if the mutex was locked
	 */
	bool lock(int timeout=-1);

	/**
	 * @brief Release the mutex
	 * This also triggers the emission of areaChanged if there were any
	 * changes while the stack was locked.
	 */
	void unlock();

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

	//! Get the index of the specified layer
	int indexOf(int id) const;

	//! Get the width of the layer stack
	int width() const { return _width; }

	//! Get the height of the layer stack
	int height() const { return _height; }

	//! Get the width and height of the layer stack
	QSize size() const { return QSize(_width, _height); }

	//! Paint all changed tiles in the given area
	void paintChangedTiles(const QRect& rect, QPaintDevice *target, bool clean=true);

	//! Get the merged color value at the point
	QColor colorAt(int x, int y, int dia=0) const;

	//! Return a flattened image of the layer stack
	QImage toFlatImage() const;

	//! Return a single layer composited with the given background
	QImage flatLayerImage(int layerIdx, bool useBgLayer, const QColor &background);

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

	//! Emit a layer info change notification
	void notifyLayerInfoChange(const Layer *layer);

	//! Create a new savepoint
	Savepoint *makeSavepoint();

	//! Restore layer stack to a previous savepoint
	void restoreSavepoint(const Savepoint *savepoint);

	//! Set layer view mode
	void setViewMode(ViewMode mode);

	ViewMode viewMode() const { return _viewmode; }

	//! Set the selected layer (used by view modes other than NORMAL)
	void setViewLayer(int id);

	//! Set onionskin view mode parameters
	void setOnionskinMode(int below, int above, bool tint);

	//! Show background layer (bottom-most layer) in special view modes
	void setViewBackgroundLayer(bool usebg);

	//! Clear the entire layer stack
	void reset();

	/** A convenience class for locking the layer stack */
	class Locker {
	public:
		Locker(LayerStack *ls) : m_layerstack(ls) {
			ls->lock();
		}
		Locker(const Locker &) = delete;
		Locker &operator=(const Locker&) = delete;
		~Locker() { m_layerstack->unlock(); }

	private:
		LayerStack *m_layerstack;
	};

signals:
	//! Emitted when the visible layers are edited
	void areaChanged(const QRect &area);

	//! Layer width/height changed
	void resized(int xoffset, int yoffset, const QSize &oldsize);

	//! A layer was just added
	void layerCreated(int idx, const LayerInfo &info);

	//! A layer was just deleted
	void layerDeleted(int idx);

	//! A single layer's info has just changed
	void layerChanged(int idx, const LayerInfo &info);

	//! All (or at least a lot of) layers have just changed
	void layersChanged(const QList<LayerInfo> &layers);

private:
	void flattenTile(quint32 *data, int xindex, int yindex) const;

	bool isVisible(int idx) const;
	int layerOpacity(int idx) const;
	quint32 layerTint(int idx) const;

	QList<LayerInfo> layerInfos() const;

	int _width, _height;
	int _xtiles, _ytiles;
	QList<Layer*> m_layers;

	QBitArray _dirtytiles;
	QRect m_dirtyrect;

	ViewMode _viewmode;
	int _viewlayeridx;
	int _onionskinsBelow, _onionskinsAbove;
	bool _onionskinTint;
	bool _viewBackgroundLayer;

	QMutex m_mutex;
	bool m_locked;
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
	int width, height;
};

}

#endif

