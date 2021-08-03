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
#ifndef LAYERSTACK_H
#define LAYERSTACK_H

#include "annotationmodel.h"
#include "tile.h"

#include <cstdint>

#include <QObject>
#include <QList>
#include <QImage>

class QDataStream;

namespace paintcore {

class LayerStackObserver;
class Layer;
class EditableLayer;
class EditableLayerStack;
class Tile;
struct Savepoint;
struct LayerInfo;

/**
 * \brief A stack of layers.
 */
class LayerStack : public QObject {
	Q_PROPERTY(AnnotationModel* annotations READ annotations CONSTANT)
	Q_OBJECT
	friend class EditableLayerStack;
	friend class LayerStackObserver;
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

	//! Get the background tile
	Tile background() const { return m_backgroundTile; }

	//! Get the number of layers in the stack
	int layerCount() const { return m_layers.count(); }

	//! Get a read only layer by its index
	const Layer *getLayerByIndex(int index) const;

	//! Get a read only layer by its ID
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

	//! Get the "last edited by" tag of the topmost visible tile
	int tileLastEditedBy(int tx, int ty) const;

	/**
	 * @brief Return a flattened image of the layer stack
	 *
	 * Note: if includeBackground is false, layers marked as *fixed* will not
	 * be included in the flattened image.
	 *
	 * @param includeAnnotations merge annotations onto the final image
	 * @param includeBackground include canvas background and fixed layers
	 */
	QImage toFlatImage(bool includeAnnotations, bool includeBackground, bool includeSublayers) const;

	/**
	 * @brief Return a single layer merged with the background
	 *
	 * Note: fixed layers are also considered background and will be included as well,
	 * even those above the target index
	 */
	QImage flatLayerImage(int layerIdx) const;

	//! Get a merged tile
	Tile getFlatTile(int x, int y) const;

	//! Create a new savepoint
	Savepoint makeSavepoint();

	//! Get the current view rendering mode
	ViewMode viewMode() const { return m_viewmode; }

	//! Are layers tagged for censoring actually censored?
	bool isCensored() const { return m_censorLayers; }

	/**
	 * @brief Set the canvas resolution (dots per inch)
	 *
	 * This information is not used internally by Drawpile, but it
	 * can be saved in files to be used by other programs.
	 */
	void setDpi(int x, int y) { m_dpix = x; m_dpiy = y; }
	void setDpi(QPair<int,int> dpi) { m_dpix = dpi.first; m_dpiy = dpi.second; }

	/**
	 * @brief Get the canvas resolution
	 *
	 * If either component is zero, the default DPI should be assumed.
	 */
	QPair<int,int> dotsPerInch() const { return QPair<int,int>(m_dpix, m_dpiy); }

	/**
	 * @brief Find a layer with a sublayer with the given ID and return its change bounds
	 * @param contextid
	 * @return Layer ID, Change bounds pair
	 */
	QPair<int,QRect> findChangeBounds(int contextid);

	//! Get a list of layer stack observers
	const QList<LayerStackObserver*> observers() const { return m_observers; }

	//! Start a layer stack editing sequence
	inline EditableLayerStack editor(int contextId);

signals:
	//! Canvas width/height changed
	void resized(int xoffset, int yoffset, const QSize &oldsize);

private:
	LayerStack(const LayerStack *orig, QObject *parent);

	// Emission of areaChanged is suppressed during an active write sequence
	void beginWriteSequence();
	void endWriteSequence();

	void flattenTile(quint32 *data, int xindex, int yindex) const;

	bool isVisible(int idx) const;
	int layerOpacity(int idx) const;
	quint32 layerTint(int idx) const;

	QList<LayerStackObserver*> m_observers;

	int m_width, m_height;
	int m_xtiles, m_ytiles;
	int m_dpix, m_dpiy;

	QList<Layer*> m_layers;
	AnnotationModel *m_annotations;

	Tile m_backgroundTile;

	ViewMode m_viewmode;
	int m_viewlayeridx;
	int m_highlightId;
	int m_onionskinsBelow, m_onionskinsAbove;
	int m_openEditors;

	bool m_onionskinTint;
	bool m_censorLayers;
};

/// Layer stack savepoint for undo use
struct Savepoint {
	Savepoint() = default;
	Savepoint(const Savepoint &other);
	~Savepoint();

	Savepoint &operator=(const Savepoint &other);

	QList<Layer*> layers;
	QList<Annotation> annotations;
	Tile background;
	QSize size;
};

/**
 * @brief A wrapper class for editing a LayerStack
 */
class EditableLayerStack {
public:
	explicit EditableLayerStack(LayerStack *layerstack, int contextId)
		: d(layerstack), contextId(contextId)
	{
		Q_ASSERT(d);
		d->beginWriteSequence();
	}
	EditableLayerStack(EditableLayerStack &&other)
	{
		d = other.d;
		other.d = nullptr;
	}

	EditableLayerStack(const EditableLayerStack&) = delete;
	EditableLayerStack &operator=(const EditableLayerStack&) = delete;

	~EditableLayerStack()
	{
		if(d)
			d->endWriteSequence();
	}

	//! Adjust layer stack size
	void resize(int top, int right, int bottom, int left);

	//! Set the background tile
	void setBackground(const Tile &tile);

	//! Create a new layer
	EditableLayer createLayer(int id, int source, const QColor &color, bool insert, bool copy, const QString &name);

	//! Delete a layer
	bool deleteLayer(int id);

	//! Merge the layer to the one below it
	void mergeLayerDown(int id);

	//! Re-order the layer stack
	void reorderLayers(const QList<uint16_t> &neworder);

	//! Get a layer by its index
	EditableLayer getEditableLayerByIndex(int index);

	//! Get a layer wrapped in EditableLayer
	EditableLayer getEditableLayer(int id);

	//! Clear the entire layer stack
	void reset();

	//! Remove all preview layers (ephemeral sublayers)
	void removePreviews();

	//! Merge all sublayers with the given ID
	void mergeSublayers(int id);

	//! Merge all sublayers with positive IDs
	void mergeAllSublayers();

	//! Set layer view mode
	void setViewMode(LayerStack::ViewMode mode);

	//! Highlight all tiles last edited by this user. If set to 0, highlighting is disabled
	void setInspectorHighlight(int contextId);

	//! Set the selected layer (used by view modes other than NORMAL)
	void setViewLayer(int id);

	//! Set onionskin view mode parameters
	void setOnionskinMode(int below, int above, bool tint);

	//! Enable/disable censoring of layers
	void setCensorship(bool censor);

	//! Restore layer stack to a previous savepoint
	void restoreSavepoint(const Savepoint &savepoint);

	const LayerStack *layerStack() const { return d; }

	const LayerStack *operator ->() const { return d; }

private:
	LayerStack *d;
	int contextId;
};

EditableLayerStack LayerStack::editor(int contextId) { return EditableLayerStack(this, contextId); }

}

#endif

