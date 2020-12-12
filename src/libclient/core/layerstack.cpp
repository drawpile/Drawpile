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

#include "layer.h"
#include "layerstack.h"
#include "layerstackobserver.h"
#include "tile.h"
#include "rasterop.h"
#include "concurrent.h"

#include <QPainter>
#include <QMimeData>
#include <QDataStream>

namespace paintcore {

static const Tile CENSORED_TILE = Tile::ZebraBlock(QColor("#232629"), QColor("#eff0f1"));
static const Tile ZEBRA_TILE = Tile::ZebraBlock(Qt::red, Qt::black, 2);

LayerStack::LayerStack(QObject *parent)
	: QObject(parent), m_width(0), m_height(0), m_xtiles(0), m_ytiles(0), m_dpix(0), m_dpiy(0),
	m_viewmode(NORMAL), m_viewlayeridx(0), m_highlightId(0),
	m_onionskinsBelow(4), m_onionskinsAbove(4), m_openEditors(0), m_onionskinTint(true), m_censorLayers(false)
{
	m_annotations = new AnnotationModel(this);
}

LayerStack::LayerStack(const LayerStack *orig, QObject *parent)
	: QObject(parent),
	  m_width(orig->m_width),
	  m_height(orig->m_height),
	  m_xtiles(orig->m_xtiles),
	  m_ytiles(orig->m_ytiles),
	  m_dpix(orig->m_dpix),
	  m_dpiy(orig->m_dpiy),
	  m_viewmode(orig->m_viewmode),
	  m_viewlayeridx(orig->m_viewlayeridx),
	  m_highlightId(orig->m_highlightId),
	  m_onionskinsBelow(orig->m_onionskinsBelow),
	  m_openEditors(0),
	  m_onionskinTint(orig->m_onionskinTint),
	  m_censorLayers(orig->m_censorLayers)
{
	m_annotations = orig->m_annotations->clone(this);
	m_backgroundTile = orig->m_backgroundTile;
	for(const Layer *l : orig->m_layers)
		m_layers << new Layer(*l);
}

LayerStack::~LayerStack()
{
	for(LayerStackObserver *observer : m_observers)
		observer->detachFromLayerStack();

	for(Layer *l : m_layers)
		delete l;
}

QPair<int,QRect> LayerStack::findChangeBounds(int contextId)
{
	for(const Layer *l : m_layers) {
		const QRect r = l->changeBounds(contextId);
		if(!r.isNull())
			return QPair<int,QRect>(l->id(), r);
	}
	return QPair<int,QRect>(0, QRect());
}

const Layer *LayerStack::getLayerByIndex(int index) const
{
	return m_layers.at(index);
}

const Layer *LayerStack::getLayer(int id) const
{
	// Since the expected number of layers is always fairly low,
	// we can get away with a simple linear search. (Note that IDs
	// may appear in random order due to layers being moved around.)
	for(const Layer *l : m_layers)
		if(l->id() == id)
			return l;
	return nullptr;
}

/**
 * @param id layer id
 * @return layer index. Returns a negative index if layer is not found
 */
int LayerStack::indexOf(int id) const
{
	for(int i=0;i<m_layers.size();++i)
		if(m_layers.at(i)->id() == id)
			return i;
	return -1;
}

Tile LayerStack::getFlatTile(int x, int y) const
{
	Tile t = m_backgroundTile;
	flattenTile(t.data(), x, y);
	return t;
}

const Layer *LayerStack::layerAt(int x, int y) const
{
	for(int i=m_layers.size()-1;i>=0;--i) {
		const Layer * l = m_layers.at(i);
		if(l->isVisible()) {
			if(l->pixelAt(x,y) > 0)
				return l;

			// Check sublayers too
			for(const Layer *sl : l->sublayers()) {
				if(sl->isVisible() && sl->pixelAt(x, y) > 0)
					return l;
			}
		}
	}
	return nullptr;
}

QColor LayerStack::colorAt(int x, int y, int dia) const
{
	if(m_layers.isEmpty())
		return QColor();

	if(x<0 || y<0 || x>=m_width || y>=m_height)
		return QColor();

	if(dia<=1) {
		// TODO some more efficient way of doing this
		Tile tile = getFlatTile(x/Tile::SIZE, y/Tile::SIZE);
		quint32 c = tile.data()[(y-Tile::roundDown(y)) * Tile::SIZE + (x-Tile::roundDown(x))];
		return QColor(c);

	} else {
		const int r = dia/2+1;
		const int x1 = qBound(0, (x-r) / Tile::SIZE, m_xtiles-1);
		const int x2 = qBound(0, (x+r) / Tile::SIZE, m_xtiles-1);
		const int y1 = qBound(0, (y-r) / Tile::SIZE, m_ytiles-1);
		const int y2 = qBound(0, (y+r) / Tile::SIZE, m_ytiles-1);

		Layer flat(0, QString(), Qt::transparent, size());
		EditableLayer ef(&flat, nullptr, 0);

		ef.putTile(0, 0, 9999*9999, m_backgroundTile);

		for(int tx=x1;tx<=x2;++tx) {
			for(int ty=y1;ty<=y2;++ty) {
				ef.putTile(tx, ty, 0, getFlatTile(tx, ty));
			}
		}

		return flat.colorAt(x, y, dia);
	}
}

int LayerStack::tileLastEditedBy(int tx, int ty) const
{
	if(tx < 0 || ty < 0 || tx >= m_xtiles || ty >= m_ytiles)
		return 0;

	for(int i=m_layers.size()-1;i>=0;--i) {
		if(isVisible(i)) {
			const Tile &t = m_layers.at(i)->tile(tx, ty);
			if(!t.isNull())
				return t.lastEditedBy();
		}
	}

	return 0;
}

QImage LayerStack::toFlatImage(bool includeAnnotations, bool includeBackground) const
{
	if(m_layers.isEmpty())
		return QImage();

	Layer flat(0, QString(), Qt::transparent, size());
	EditableLayer ef(&flat, nullptr, 0);

	if(includeBackground)
		ef.putTile(0, 0, 9999*9999, m_backgroundTile);

	for(int i=0;i<m_layers.size();++i) {
		if(m_layers.at(i)->isVisible() && (includeBackground || !m_layers.at(i)->isFixed()))
			ef.merge(m_layers.at(i));
	}

	QImage image = ef->toImage();

	if(includeAnnotations) {
		QPainter painter(&image);
		for(const Annotation &a : m_annotations->getAnnotations())
			a.paint(&painter);
	}

	if(m_dpix > 0 && m_dpiy > 0) {
		image.setDotsPerMeterX(int(m_dpix / 0.0254));
		image.setDotsPerMeterY(int(m_dpiy / 0.0254));
	}
	return image;
}

QImage LayerStack::flatLayerImage(int layerIdx) const
{
	Q_ASSERT(layerIdx>=0 && layerIdx < m_layers.size());

	Layer flat(0, QString(), Qt::transparent, size());
	EditableLayer ef(&flat, nullptr, 0);

	ef.putTile(0, 0, 9999*9999, m_backgroundTile);

	for(int i=0;i<m_layers.size();++i) {
		if(i == layerIdx || m_layers.at(i)->isFixed())
			ef.merge(m_layers.at(i));
	}

	QImage image = ef->toImage();
	if(m_dpix > 0 && m_dpiy > 0) {
		image.setDotsPerMeterX(int(m_dpix / 0.0254));
		image.setDotsPerMeterY(int(m_dpiy / 0.0254));
	}

	return image;
}

// Flatten a single tile
void LayerStack::flattenTile(quint32 *data, int xindex, int yindex) const
{
	// Composite visible layers
	int layeridx = 0;
	for(const Layer *l : m_layers) {
		if(isVisible(layeridx)) {
			const Tile &tile = l->tile(xindex, yindex);
			const quint32 tint = layerTint(layeridx);

			if(m_censorLayers && l->isCensored()) {
				// This layer must be censored
				if(!tile.isNull())
					compositePixels(l->blendmode(), data, CENSORED_TILE.constData(),
							Tile::LENGTH, layerOpacity(layeridx));

			} else if(l->sublayers().count() || tint!=0 || m_highlightId > 0) {
				// Sublayers (or tint) present, composite them first
				quint32 ldata[Tile::SIZE*Tile::SIZE];
				tile.copyTo(ldata);

				for(const Layer *sl : l->sublayers()) {
					if(sl->isVisible()) {
						const Tile &subtile = sl->tile(xindex, yindex);
						if(!subtile.isNull()) {
							compositePixels(sl->blendmode(), ldata, subtile.constData(),
									Tile::LENGTH, sl->opacity());
						}
					}
				}

				if(m_highlightId > 0 && m_highlightId == tile.lastEditedBy()) {
					// MODE_RECOLOR looks really nice here, but can be misleading.
					// Use per-pixel highlighting if/when per-pixel tagging is implemented.
					compositePixels(BlendMode::MODE_NORMAL, ldata, ZEBRA_TILE.constData(),
							Tile::LENGTH, 128);
				}

				if(tint)
					tintPixels(ldata, sizeof ldata / sizeof *ldata, tint);


				// Composite merged tile
				compositePixels(l->blendmode(), data, ldata,
						Tile::SIZE*Tile::SIZE, layerOpacity(layeridx));

			} else if(!tile.isNull()) {
				// No sublayers or tint, just this tile as it is
				compositePixels(l->blendmode(), data, tile.constData(),
						Tile::LENGTH, layerOpacity(layeridx));
			}
		}

		++layeridx;
	}
}

void LayerStack::beginWriteSequence()
{
	++m_openEditors;
}

void LayerStack::endWriteSequence()
{
	--m_openEditors;
	Q_ASSERT(m_openEditors>=0);
	if(m_openEditors == 0) {
		for(auto observer : m_observers)
			observer->canvasWriteSequenceDone();
	}
}

int LayerStack::layerOpacity(int idx) const
{
	Q_ASSERT(idx>=0 && idx < m_layers.size());
	int o = m_layers.at(idx)->opacity();

	if(viewMode()==ONIONSKIN && !m_layers.at(idx)->isFixed()) {

		const int d = m_viewlayeridx - idx;
		qreal rd;
		if(d<0 && d>=-m_onionskinsAbove)
			rd = -d/qreal(m_onionskinsAbove+1);
		else if(d>=0 && d <=m_onionskinsBelow)
			rd = d/qreal(m_onionskinsBelow+1);
		else
			return 0;

		return int(o * ((1-rd) * (1-rd)));
	}

	return o;
}

quint32 LayerStack::layerTint(int idx) const
{
	if(m_onionskinTint && viewMode() == ONIONSKIN && !m_layers.at(idx)->isFixed()) {
		if(idx < m_viewlayeridx)
			return 0x80ff3333;
		else if(idx > m_viewlayeridx)
			return 0x803333ff;
	}

	return 0;
}

bool LayerStack::isVisible(int idx) const
{
	Q_ASSERT(idx>=0 && idx < m_layers.size());
	if(!m_layers.at(idx)->isVisible())
		return false;

	switch(viewMode()) {
	case NORMAL: break;
	case SOLO: return idx == m_viewlayeridx || m_layers.at(idx)->isFixed();
	case ONIONSKIN: return layerOpacity(idx) > 0;
	}

	return true;
}

Savepoint LayerStack::makeSavepoint()
{
	Savepoint sp;
	for(Layer *l : m_layers) {
		l->optimize();
		sp.layers.append(new Layer(*l));
	}

	sp.annotations = m_annotations->getAnnotations();
	sp.background = m_backgroundTile;

	sp.size = size();

	return sp;
}

Savepoint::Savepoint(const Savepoint &other)
{
	for(Layer *l : other.layers)
		layers << new Layer(*l);
	annotations = other.annotations;
	background = other.background;
	size = other.size;
}

Savepoint &Savepoint::operator=(const Savepoint &other)
{
	if(&other != this) {
		for(Layer *l : layers)
			delete l;
		layers.clear();
		for(Layer *l : other.layers)
			layers << new Layer(*l);
		annotations = other.annotations;
		background = other.background;
		size = other.size;
	}
	return *this;
}

Savepoint::~Savepoint()
{
	for(Layer *l : layers)
		delete l;
}

void EditableLayerStack::restoreSavepoint(const Savepoint &savepoint)
{
	const QSize oldsize(d->m_width, d->m_height);
	if(d->width() != savepoint.size.width() || d->height() != savepoint.size.height()) {
		// Restore canvas size if it was different in the savepoint
		d->m_width = savepoint.size.width();
		d->m_height = savepoint.size.height();
		d->m_xtiles = Tile::roundTiles(d->m_width);
		d->m_ytiles = Tile::roundTiles(d->m_height);
		for(auto observer : d->m_observers)
			observer->canvasResized(0, 0, oldsize);
		emit d->resized(0, 0, oldsize);

	} else {
		// Mark changed tiles as changed. Usually savepoints are quite close together
		// so most tiles will remain unchanged
		if(savepoint.layers.size() != d->m_layers.size()) {
			// Layers added or deleted, just refresh everything
			// (force refresh even if layer stack is empty)
			for(auto observer : d->m_observers)
				observer->markDirty();

		} else {
			// Layer count has not changed, compare layer contents
			for(int l=0;l<savepoint.layers.size();++l) {
				const Layer *l0 = d->m_layers.at(l);
				const Layer *l1 = savepoint.layers.at(l);
				if(l0->effectiveOpacity() != l1->effectiveOpacity()) {
					// Layer opacity has changed, refresh everything
					for(auto observer : d->m_observers)
						observer->markDirty();
					break;
				}

				// Gather list of sublayer IDs to compare
				QVarLengthArray<int, 10> sublayers;
				for(const Layer *sl : l0->sublayers())
					if(sl->id() > 0 && !sl->isHidden() && !sublayers.contains(sl->id()))
						sublayers << sl->id();

				for(const Layer *sl : l1->sublayers())
					if(sl->id() > 0 && !sl->isHidden() && !sublayers.contains(sl->id()))
						sublayers << sl->id();

				// Compare sublayers
				const int tilecount = d->m_xtiles * d->m_ytiles;

				for(int sublayerId : sublayers) {
					const Layer *sl0 = l0->getVisibleSublayer(sublayerId);
					const Layer *sl1 = l1->getVisibleSublayer(sublayerId);
					const Layer *delta = nullptr;

					if(sl0) {
						if(sl1) {
							// Visible in both, compare content
							for(int i=0;i<tilecount;++i) {
								// Note: An identity comparison works here, because the tiles
								// utilize copy-on-write semantics. Unchanged tiles will share
								// data pointers between savepoints.
								if(sl0->tile(i) != sl1->tile(i)) {
									for(auto observer : d->m_observers)
										observer->markDirty(i);
								}
							}
						} else {
							// Not visible in sl1
							delta = sl0;
						}
					} else {
						// Not visible in sl0, therefore must be visible in sl1
						delta = sl1;
					}

					if(delta) {
						// Visible in one but not both: mark opaque areas as dirty
						for(int i=0;i<tilecount;++i) {
							if(!delta->tile(i).isNull())
								for(auto observer : d->m_observers)
									observer->markDirty(i);
						}
					}
				}

				// Compare the main layer
				for(int i=0;i<d->m_xtiles*d->m_ytiles;++i) {
					// Note: An identity comparison works here, because the tiles
					// utilize copy-on-write semantics. Unchanged tiles will share
					// data pointers between savepoints.
					if(l0->tile(i) != l1->tile(i)) {
						for(auto observer : d->m_observers)
							observer->markDirty(i);
					}
				}
			}
		}
	}

	// Restore layers
	while(!d->m_layers.isEmpty())
		delete d->m_layers.takeLast();
	for(const Layer *l : savepoint.layers)
		d->m_layers.append(new Layer(*l));

	// Restore background
	setBackground(savepoint.background);

	// Restore annotations
	d->m_annotations->setAnnotations(savepoint.annotations);
}

void EditableLayerStack::resize(int top, int right, int bottom, int left)
{
	const QSize oldsize(d->m_width, d->m_height);

	int newtop = -top;
	int newleft = -left;
	int newright = d->m_width + right;
	int newbottom = d->m_height + bottom;
	if(newtop >= newbottom || newleft >= newright) {
		qWarning("Invalid resize: borders reversed");
		return;
	}
	d->m_width = newright - newleft;
	d->m_height = newbottom - newtop;

	d->m_xtiles = Tile::roundTiles(d->m_width);
	d->m_ytiles = Tile::roundTiles(d->m_height);

	for(Layer *l : d->m_layers)
		EditableLayer(l, d, contextId).resize(top, right, bottom, left);

	if(left || top) {
		// Update annotation positions
		QPoint offset(left, top);
		for(const Annotation &a : d->m_annotations->getAnnotations()) {
			d->m_annotations->reshapeAnnotation(a.id, a.rect.translated(offset));
		}
	}

	for(auto observer : d->m_observers)
		observer->canvasResized(left, top, oldsize);

	emit d->resized(left, top, oldsize);
}

void EditableLayerStack::setBackground(const Tile &tile)
{
	if(tile.equals(d->m_backgroundTile))
		return;

	d->m_backgroundTile = tile;

	for(auto observer : d->m_observers)
		observer->canvasBackgroundChanged(tile);
}

/**
 * @param id ID of the new layer
 * @param source source layer ID (used when copy or insert is true)
 * @param color background color (used when copy is false)
 * @param insert if true, the new layer is inserted above source (source 0 inserts at the bottom of the stack)
 * @param copy if true, the layer content is copied from the source
 * @param name layer title
 * @return newly created layer or null in case of error
 */
EditableLayer EditableLayerStack::createLayer(int id, int source, const QColor &color, bool insert, bool copy, const QString &name)
{
	if(d->getLayer(id)) {
		qWarning("Layer #%d already exists!", id);
		return EditableLayer();
	}

	if(d->m_width<=0 || d->m_height<=0) {
		// We tolerate this, but in normal operation the canvas size should be
		// set before creating any layers.
		qWarning("Layer created before canvas size was set!");
	}

	// Find source layer if specified
	int sourceIdx=-1;
	if(source>0) {
		for(int i=0;i<d->m_layers.size();++i) {
			if(d->m_layers.at(i)->id() == source) {
				sourceIdx = i;
				break;
			}
		}

		if(sourceIdx<0) {
			qWarning("Source layer %d not found!", source);
			return EditableLayer();
		}
	}

	// Create or copy new layer
	Layer *nl;
	if(copy) {
		if(sourceIdx<0) {
			qWarning("No layer copy source specified!");
			return EditableLayer();
		}

		nl = new Layer(*d->m_layers.at(sourceIdx));
		EditableLayer enl(nl, nullptr, 0);
		enl.setTitle(name);
		enl.setId(id);

	} else {
		nl = new Layer(id, name, color, d->size());
	}

	// Insert the new layer in the appropriate spot
	int pos;
	if(insert)
		pos = sourceIdx+1;
	else
		pos = d->m_layers.size();

	d->m_layers.insert(pos, nl);

	// Dirty regions must be marked after the layer is in the stack
	EditableLayer editable(nl, d, 0);

	if(copy) {
		editable.markOpaqueDirty();

	} else if(color.alpha()>0) {
		for(auto observer : d->m_observers)
			observer->markDirty();
	}

	return editable;
}

/**
 * @param id layer ID
 * @return true if layer was found and deleted
 */
bool EditableLayerStack::deleteLayer(int id)
{
	for(int i=0;i<d->m_layers.size();++i) {
		if(d->m_layers.at(i)->id() == id) {
			EditableLayer(d->m_layers.at(i), d, contextId).markOpaqueDirty();
			delete d->m_layers.takeAt(i);

			return true;
		}
	}
	return false;
}

/**
 * @param neworder list of layer IDs in the new order
 * @pre neworder must be a permutation of the current layer order
 */
void EditableLayerStack::reorderLayers(const QList<uint16_t> &neworder)
{
	Q_ASSERT(neworder.size() == d->m_layers.size());
	QList<Layer*> newstack;
	newstack.reserve(d->m_layers.size());
	for(const int id : neworder) {
		Layer *l = nullptr;
		for(int i=0;i<d->m_layers.size();++i) {
			if(d->m_layers.at(i)->id() == id) {
				l=d->m_layers.takeAt(i);
				break;
			}
		}
		Q_ASSERT(l);
		newstack.append(l);
	}
	d->m_layers = newstack;
	for(auto observer : d->m_observers)
		observer->markDirty();
}

/**
 * @param id id of the layer that will be merged
 */
void EditableLayerStack::mergeLayerDown(int id) {
	const Layer *top;
	Layer *btm=nullptr;
	for(int i=0;i<d->m_layers.size();++i) {
		if(d->m_layers[i]->id() == id) {
			top = d->m_layers[i];
			if(i>0)
				btm = d->m_layers[i-1];
			break;
		}
	}
	if(btm)
		EditableLayer(btm, d, contextId).merge(top);
	else
		qWarning("Tried to merge bottom-most layer");
}

EditableLayer EditableLayerStack::getEditableLayerByIndex(int index)
{
	return EditableLayer(d->m_layers[index], d, contextId);
}

EditableLayer EditableLayerStack::getEditableLayer(int id)
{
	for(Layer *l : d->m_layers)
		if(l->id() == id)
			return EditableLayer(l, d, contextId);
	return EditableLayer();
}

void EditableLayerStack::reset()
{
	const QSize oldsize(d->m_width, d->m_height);
	d->m_width = 0;
	d->m_height = 0;
	d->m_xtiles = 0;
	d->m_ytiles = 0;
	for(Layer *l : d->m_layers)
		delete l;
	d->m_layers.clear();
	d->m_annotations->clear();

	d->m_backgroundTile = Tile();

	for(auto *observer : d->m_observers) {
		observer->canvasResized(0, 0, oldsize);
		observer->canvasBackgroundChanged(Tile());
	}

	emit d->resized(0, 0, oldsize);
}

void EditableLayerStack::removePreviews()
{
	for(Layer *l : d->m_layers) {
		EditableLayer el(l, d, contextId);
		el.removePreviews();
		el.markOpaqueDirty();
	}
}

void EditableLayerStack::mergeSublayers(int id)
{
	for(Layer *l : d->m_layers) {
		EditableLayer(l, d, contextId).mergeSublayer(id);
	}
}

void EditableLayerStack::mergeAllSublayers()
{
	for(Layer *l : d->m_layers) {
		EditableLayer(l, d, contextId).mergeAllSublayers();
	}
}

void EditableLayerStack::setViewMode(LayerStack::ViewMode mode)
{
	if(mode != d->m_viewmode) {
		d->m_viewmode = mode;
		for(auto observer : d->m_observers)
			observer->markDirty();
	}
}

void EditableLayerStack::setViewLayer(int id)
{
	for(int i=0;i<d->m_layers.size();++i) {
		if(d->m_layers.at(i)->id() == id) {
			d->m_viewlayeridx = i;
			if(d->m_viewmode != LayerStack::NORMAL) {
				for(auto observer : d->m_observers)
					observer->markDirty();
			}
			break;
		}
	}
}

void EditableLayerStack::setInspectorHighlight(int contextId)
{
	if(d->m_highlightId != contextId) {
		d->m_highlightId = contextId;
		for(auto observer : d->m_observers)
			observer->markDirty();
	}
}

void EditableLayerStack::setOnionskinMode(int below, int above, bool tint)
{
	d->m_onionskinsBelow = below;
	d->m_onionskinsAbove = above;
	d->m_onionskinTint = tint;

	if(d->m_viewmode == LayerStack::ONIONSKIN) {
		for(auto observer : d->m_observers)
			observer->markDirty();
	}
}

void EditableLayerStack::setCensorship(bool censor)
{
	if(d->m_censorLayers != censor) {
		d->m_censorLayers = censor;
		// We could check if this really needs to be called, but this
		// flag is changed very infrequently
		for(auto observer : d->m_observers)
			observer->markDirty();
	}
}

}

