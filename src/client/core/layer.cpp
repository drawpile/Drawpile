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

#include "layerstack.h"
#include "layerstackobserver.h"
#include "layer.h"
#include "tile.h"
#include "brushmask.h"
#include "point.h"
#include "blendmodes.h"
#include "rasterop.h"
#include "concurrent.h"

#include <QPainter>
#include <QImage>
#include <QDataStream>
#include <cmath>

#define OBSERVERS(notification) for(auto *observer : owner->observers()) observer->notification

namespace paintcore {

namespace {

//! Sample colors at layer edges and return the most frequent color
QColor _sampleEdgeColors(const Layer *layer, bool top, bool right, bool bottom, bool left)
{
	const int STEP = 22;
	QHash<QRgb, int> colorfreq;
	// Sample top & bottom edges
	for(int x=0;x<layer->width();x+=STEP) {
		int count;
		QRgb c;
		if(top) {
			c = layer->pixelAt(x, 0);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}

		if(bottom) {
			c = layer->pixelAt(x, layer->height()-1);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}
	}

	// Sample left & right edges
	for(int y=0;y<layer->height();y+=STEP) {
		int count;
		QRgb c;
		if(left) {
			c = layer->pixelAt(0, y);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}

		if(right) {
			c = layer->pixelAt(layer->width()-1, y);
			count = colorfreq[c];
			++count;
			colorfreq[c] = count;
		}
	}

	// Return the most frequent color
	QRgb color=0;
	int freq=0;
	QHashIterator<QRgb, int> i(colorfreq);
	while(i.hasNext()) {
		i.next();
		if(i.value() > freq) {
			freq = i.value();
			color = i.key();
		}
	}

	return QColor::fromRgba(qUnpremultiply(color));
}

}

/**
 * Construct a layer initialized to a solid color
 * @param owner the stack to which this layer belongs to
 * @param id layer ID
 * @param color layer color
 * @parma size layer size
 */
Layer::Layer(int id, const QString& title, const QColor& color, const QSize& size)
	: m_info({id, title}),
	  m_width(size.width()),
	  m_height(size.height()),
	  m_xtiles(Tile::roundTiles(size.width())),
	  m_ytiles(Tile::roundTiles(size.height()))
{
	m_tiles = QVector<Tile>(
		m_xtiles * m_ytiles,
		color.alpha() > 0 ? Tile(color) : Tile()
	);
}

Layer::Layer(int id, const QSize &size)
	: Layer(id, QString(), Qt::transparent, size)
{
	// sublayers are used for indirect drawing and previews
}

Layer::Layer(const Layer &layer)
	: m_info(layer.m_info),
	  m_changeBounds(layer.m_changeBounds),
	  m_tiles(layer.m_tiles),
	  m_width(layer.m_width), m_height(layer.m_height),
	  m_xtiles(layer.m_xtiles), m_ytiles(layer.m_ytiles)
{
	// Hidden and ephemeral layers are not copied, since hiding a sublayer is
	// effectively the same as deleting it and ephemeral layers are not considered
	// part of the true layer content.
	for(const Layer *sl : layer.sublayers()) {
		if(sl->id() >= 0 && !sl->isHidden())
			m_sublayers.append(new Layer(*sl));
	}
}

Layer::~Layer() {
	for(Layer *sl : m_sublayers)
		delete sl;
}

QImage Layer::toImage() const {
	QImage image(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
	int i=0;
	for(int y=0;y<m_ytiles;++y) {
		for(int x=0;x<m_xtiles;++x,++i)
			m_tiles[i].copyToImage(image, x*Tile::SIZE, y*Tile::SIZE);
	}
	return image;
}

QImage Layer::toCroppedImage(int *xOffset, int *yOffset) const
{
	int top=m_ytiles, bottom=0;
	int left=m_xtiles, right=0;

	// Find bounding rectangle of non-blank tiles
	for(int y=0;y<m_ytiles;++y) {
		for(int x=0;x<m_xtiles;++x) {
			const Tile &t = m_tiles.at(y*m_xtiles + x);
			if(!t.isBlank()) {
				if(x<left)
					left=x;
				if(x>right)
					right=x;
				if(y<top)
					top=y;
				if(y>bottom)
					bottom=y;
			}
		}
	}

	if(top==m_ytiles) {
		// Entire layer appears to be blank
		return QImage();
	}

	// Copy tiles to image
	QImage image((right-left+1)*Tile::SIZE, (bottom-top+1)*Tile::SIZE, QImage::Format_ARGB32_Premultiplied);
	for(int y=top;y<=bottom;++y) {
		for(int x=left;x<=right;++x) {
			m_tiles.at(y*m_xtiles+x).copyToImage(image, (x-left)*Tile::SIZE, (y-top)*Tile::SIZE);
		}
	}

	if(xOffset)
		*xOffset = left * Tile::SIZE;
	if(yOffset)
		*yOffset = top * Tile::SIZE;

	// TODO pixel perfect cropping
	return image;
}

/**
 * @param x
 * @param y
 * @return invalid color if x or y is outside image boundaries
 */
QColor Layer::colorAt(int x, int y, int dia) const
{
	if(x<0 || y<0 || x>=m_width || y>=m_height)
		return QColor();

	if(dia<2) {
		const quint32 c = pixelAt(x, y);
		if(c==0)
			return QColor();

		return QColor::fromRgb(qUnpremultiply(c));

	} else {
		return getDabColor(makeColorSamplingStamp(dia/2, QPoint(x,y)));
	}
}

QRgb Layer::pixelAt(int x, int y) const
{
	if(x<0 || y<0 || x>=m_width || y>=m_height)
		return 0;

	const int yindex = y/Tile::SIZE;
	const int xindex = x/Tile::SIZE;

	return tile(xindex, yindex).pixel(x-xindex*Tile::SIZE, y-yindex*Tile::SIZE);
}

/**
 * Return a temporary layer with the original image padded and composited with the
 * content of this layer.
 *
 * @param xpos target image position
 * @param ypos target image position
 * @param original the image to pad
 * @param mode compositing mode
 * @param contextId the ID to assign as the "last edited by" tag
 */
Layer Layer::padImageToTileBoundary(int xpos, int ypos, const QImage &original, BlendMode::Mode mode, int contextId) const
{
	const int x0 = Tile::roundDown(xpos);
	const int x1 = qMin(m_width, Tile::roundUp(xpos+original.width()));
	const int y0 = Tile::roundDown(ypos);
	const int y1 = qMin(m_height, Tile::roundUp(ypos+original.height()));

	const int w = x1 - x0;
	const int h = y1 - y0;
	
	// Pad the image to tile boundaries
	QImage image;
	if(image.width() == w && image.height() == h) {
		image = original;

	} else {
		image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
		QPainter painter(&image);

		if(mode == BlendMode::MODE_REPLACE) {
			// Replace mode is special: we must copy the original pixels
			// and do the composition with QPainter, since layer merge
			// can't distinguish the padding from image transparency
			for(int y=0;y<h;y+=Tile::SIZE) {
				for(int x=0;x<w;x+=Tile::SIZE) {
					tile((x0+x) / Tile::SIZE, (y0+y) / Tile::SIZE).copyToImage(image, x, y);
				}
			}
			painter.setCompositionMode(QPainter::CompositionMode_Source);

		} else {
			image.fill(0);
		}

		painter.drawImage(xpos-x0, ypos-y0, original);
#if 0 /* debugging aid */
		painter.setPen(Qt::red);
		painter.drawLine(0, 0, w-1, 0);
		painter.drawLine(0, 0, 0, h-1);
		painter.drawLine(w-1, 0, w-1, h-1);
		painter.drawLine(0, h-1, w-1, h-1);
#endif
	}

	// Create scratch layers and composite
	Layer scratch(0, QString(), Qt::transparent, QSize(w,h));
	Layer imglayer = scratch;

	// Copy image pixels to image layer
	for(int y=0;y<h;y+=Tile::SIZE) {
		for(int x=0;x<w;x+=Tile::SIZE) {
			imglayer.rtile(x/Tile::SIZE, y/Tile::SIZE) = Tile(image, x, y, contextId);
		}
	}

	// In replace mode, compositing was already done with QPainter
	if(mode == BlendMode::MODE_REPLACE) {
		return imglayer;
	}

	// Copy tiles from current layer to scratch layer
	for(int y=0;y<h;y+=Tile::SIZE) {
		for(int x=0;x<w;x+=Tile::SIZE) {
			scratch.rtile(x/Tile::SIZE, y/Tile::SIZE) = tile((x+x0)/Tile::SIZE, (y+y0)/Tile::SIZE);
			scratch.rtile(x/Tile::SIZE, y/Tile::SIZE).setLastEditedBy(contextId);
		}
	}

	// Merge image using standard layer compositing ops
	EditableLayer(&imglayer, nullptr, 0).setBlend(mode);
	EditableLayer(&scratch, nullptr, 0).merge(&imglayer);

	return scratch;
}

/**
 * @brief Get a weighted average of the layer's color, using the given brush mask as the weight
 *
 * @param stamp
 * @return color average
 */
QColor Layer::getDabColor(const BrushStamp &stamp) const
{
	// This is very much like directDab, instead we only read pixel values
	const uchar *weights = stamp.mask.data();

	// The mask can overlap multiple tiles
	const int dia = stamp.mask.diameter();
	const int bottom = qMin(stamp.top + dia, m_height);
	const int right = qMin(stamp.left + dia, m_width);

	int y = qMax(0, stamp.top);
	int yb = stamp.top<0?-stamp.top:0; // y in relation to brush origin
	const int x0 = qMax(0, stamp.left);
	const int xb0 = stamp.left<0?-stamp.left:0;

	qreal weight=0, red=0, green=0, blue=0, alpha=0;

	// collect weighted color sums
	while(y<bottom) {
		const int yindex = y / Tile::SIZE;
		const int yt = y - yindex * Tile::SIZE;
		const int hb = yt+dia-yb < Tile::SIZE ? dia-yb : Tile::SIZE-yt;
		int x = x0;
		int xb = xb0; // x in relation to brush origin
		while(x<right) {
			const int xindex = x / Tile::SIZE;
			const int xt = x - xindex * Tile::SIZE;
			const int wb = xt+dia-xb < Tile::SIZE ? dia-xb : Tile::SIZE-xt;
			const int i = m_xtiles * yindex + xindex;

			std::array<quint32, 5> avg = m_tiles.at(i).weightedAverage(weights + yb * dia + xb, xt, yt, wb, hb, dia-wb);
			weight += avg[0];
			red += avg[1];
			green += avg[2];
			blue += avg[3];
			alpha += avg[4];

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}

	// Calculate final average
	red /= weight;
	green /= weight;
	blue /= weight;
	alpha /= weight;

	// Unpremultiply
	if(alpha>0) {
		red = qMin(1.0, red/alpha);
		green = qMin(1.0, green/alpha);
		blue = qMin(1.0, blue/alpha);
	}

	return QColor::fromRgbF(red, green, blue, alpha);
}

/**
 * Free all tiles that are completely transparent
 */
void Layer::optimize()
{
	// Optimize tile memory usage
	for(int i=0;i<m_tiles.size();++i) {
		if(!m_tiles[i].isNull() && m_tiles[i].isBlank())
			m_tiles[i] = Tile();
	}

	// Delete unused sublayers
	QMutableListIterator<Layer*> li(m_sublayers);
	while(li.hasNext()) {
		Layer *sl = li.next();
		if(sl->isHidden()) {
			delete sl;
			li.remove();
		}
	}
}

Layer *Layer::getSubLayer(int id, BlendMode::Mode blendmode, uchar opacity)
{
	Q_ASSERT(id != 0);

	// See if the sublayer exists already
	for(Layer *sl : m_sublayers)
		if(sl->id() == id) {
			if(sl->isHidden()) {
				// Hidden, reset properties
				sl->m_tiles.fill(Tile());
				sl->m_info.opacity = opacity;
				sl->m_info.blend = blendmode;
				sl->m_info.hidden = false;
				sl->m_changeBounds = QRect();
			}
			return sl;
		}

	// Okay, try recycling a sublayer
	for(Layer *sl : m_sublayers) {
		if(sl->isHidden()) {
			// Set these flags directly to avoid markDirty call.
			// We know the layer is invisible at this point
			sl->m_tiles.fill(Tile());
			sl->m_info.id = id;
			sl->m_info.opacity = opacity;
			sl->m_info.blend = blendmode;
			sl->m_info.hidden = false;
			return sl;
		}
	}

	// No available sublayers, create a new one
	Layer *sl = new Layer(id, QSize(m_width, m_height));
	sl->m_info.opacity = opacity;
	sl->m_info.blend = blendmode;
	m_sublayers.append(sl);
	return sl;
}

void Layer::toDatastream(QDataStream &out) const
{
	// Write Layer metadata
	out << qint32(id());
	out << quint32(width()) << quint32(height());
	out << m_info.title;
	out << m_info.opacity;
	out << quint8(m_info.blend);
	out << m_info.hidden;

	// Write layer content
	for(const Tile &t : m_tiles)
		out << t;

	// Write sublayers
	out << quint8(m_sublayers.size());
	for(const Layer *sl : m_sublayers) {
		sl->toDatastream(out);
	}
}

Layer *Layer::fromDatastream(QDataStream &in)
{
	// Read metadata
	qint32 id;
	in >> id;

	quint32 lw, lh;
	in >> lw >> lh;

	QString title;
	in >> title;

	uchar opacity;
	uchar blend;
	bool hidden;
	in >> opacity >> blend >> hidden;

	// Read tiles
	Layer *layer = new Layer(id, title, Qt::transparent, QSize(lw, lh));

	for(Tile &t : layer->m_tiles)
		in >> t;

	layer->m_info.opacity = opacity;
	layer->m_info.blend = BlendMode::Mode(blend);
	layer->m_info.hidden = hidden;

	// Read sublayers
	quint8 sublayers;
	in >> sublayers;
	while(sublayers--) {
		Layer *sl = Layer::fromDatastream(in);
		if(!sl) {
			delete layer;
			return 0;
		}
		layer->m_sublayers.append(sl);
	}

	return layer;
}

void EditableLayer::resize(int top, int right, int bottom, int left)
{
	Q_ASSERT(d);

	// Minimize amount of data that needs to be copied
	d->optimize();

	// Resize sublayers
	for(Layer *sl : d->m_sublayers)
		EditableLayer(sl, nullptr, contextId).resize(top, right, bottom, left);

	// Calculate new size
	int width = left + d->m_width + right;
	int height = top + d->m_height + bottom;

	int xtiles = Tile::roundTiles(width);
	int ytiles = Tile::roundTiles(height);
	QVector<Tile> tiles(xtiles * ytiles);

	// if there is no old content, resizing is simple
	bool hascontent = false;
	for(int i=0;i<d->m_tiles.size();++i) {
		if(!d->m_tiles.at(i).isBlank()) {
			hascontent = true;
			break;
		}
	}
	if(!hascontent) {
		d->m_width = width;
		d->m_height = height;
		d->m_xtiles = xtiles;
		d->m_ytiles = ytiles;
		d->m_tiles = tiles;
		return;
	}

	// Sample colors around the layer edges to determine fill color
	// for the new tiles
	Tile bgtile;
	{
		QColor bgcolor = _sampleEdgeColors(d, top>0, right>0, bottom>0, left>0);
		if(bgcolor.alpha()>0)
			bgtile = Tile(bgcolor);
	}

	if((left % Tile::SIZE) || (top % Tile::SIZE)) {
		// If top/left adjustment is not divisble by tile size,
		// we need to move the layer content

		QImage oldcontent = d->toImage();

		d->m_width = width;
		d->m_height = height;
		d->m_xtiles = xtiles;
		d->m_ytiles = ytiles;
		d->m_tiles = tiles;
		if(left<0 || top<0) {
			int cropx = 0;
			if(left<0) {
				cropx = -left;
				left = 0;
			}
			int cropy = 0;
			if(top<0) {
				cropy = -top;
				top = 0;
			}
			oldcontent = oldcontent.copy(cropx, cropy, oldcontent.width()-cropx, oldcontent.height()-cropy);
		}

		d->m_tiles.fill(bgtile);

		putImage(left, top, oldcontent, BlendMode::MODE_REPLACE);

	} else {
		// top/left offset is aligned at tile boundary:
		// existing tile content can be reused

		const int firstrow = Tile::roundTiles(-top);
		const int firstcol = Tile::roundTiles(-left);

		int oldy = firstrow;
		for(int y=0;y<ytiles;++y,++oldy) {
			int oldx = firstcol;
			const int oldyy = d->m_xtiles * oldy;
			const int yy = xtiles * y;
			for(int x=0;x<xtiles;++x,++oldx) {
				const int i = yy + x;

				if(oldy<0 || oldy>=d->m_ytiles || oldx<0 || oldx>=d->m_xtiles) {
					tiles[i] = bgtile;

				} else {
					const int oldi = oldyy + oldx;
					tiles[i] = d->m_tiles.at(oldi);
				}
			}
		}

		d->m_width = width;
		d->m_height = height;
		d->m_xtiles = xtiles;
		d->m_ytiles = ytiles;
		d->m_tiles = tiles;
	}
}

/**
 * @param opacity
 */
void EditableLayer::setOpacity(int opacity)
{
	Q_ASSERT(d);
	Q_ASSERT(opacity>=0 && opacity<256);
	if(d->m_info.opacity != opacity) {
		d->m_info.opacity = opacity;
		markOpaqueDirty(true);
	}
}

void EditableLayer::setBlend(BlendMode::Mode blend)
{
	Q_ASSERT(d);
	if(d->m_info.blend != blend) {
		d->m_info.blend = blend;
		markOpaqueDirty();
	}
}

void EditableLayer::setCensored(bool censor)
{
	Q_ASSERT(d);
	if(d->m_info.censored != censor) {
		d->m_info.censored = censor;
		markOpaqueDirty(true);
	}
}

void EditableLayer::setFixed(bool fixed)
{
	Q_ASSERT(d);
	if(d->m_info.fixed != fixed) {
		d->m_info.fixed = fixed;
		markOpaqueDirty(true);
	}
}

/**
 * @param hide new status
 */
void EditableLayer::setHidden(bool hide)
{
	Q_ASSERT(d);
	if(d->m_info.hidden != hide) {
		d->m_info.hidden = hide;
		markOpaqueDirty(true);
	}
}

/**
 * @param x x coordinate
 * @param y y coordinate
 * @param image the image to draw
 * @param mode blending/compositing mode (see protocol::PutImage)
 */
void EditableLayer::putImage(int x, int y, QImage image, BlendMode::Mode mode)
{
	Q_ASSERT(d);
	Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);
	
	// Check if the image is completely outside the layer
	if(x >= d->m_width || y >= d->m_height || x+image.width() < 0 || y+image.height() < 0)
		return;

	// Crop image if x or y are negative
	if(x<0 || y<0) {
		const int xCrop = x<0 ? -x : 0;
		const int yCrop = y<0 ? -y : 0;

		image = image.copy(xCrop, yCrop, image.width()-xCrop, image.height()-yCrop);
		x = qMax(0, x);
		y = qMax(0, y);
	}

	const int x0 = Tile::roundDown(x);
	const int y0 = Tile::roundDown(y);
	
	const Layer imageLayer = d->padImageToTileBoundary(x, y, image, mode, contextId);

	// Replace this layer's tiles with the scratch tiles
	const int tx0 = x0 / Tile::SIZE;
	const int ty0 = y0 / Tile::SIZE;
	const int tx1 = qMin((x0 + imageLayer.width() - 1) / Tile::SIZE, d->m_xtiles-1);
	const int ty1 = qMin((y0 + imageLayer.height() - 1) / Tile::SIZE, d->m_ytiles-1);

	for(int ty=ty0;ty<=ty1;++ty) {
		for(int tx=tx0;tx<=tx1;++tx) {
			d->rtile(tx, ty) = imageLayer.tile(tx-tx0, ty-ty0);
		}
	}

	if(owner && d->isVisible())
		OBSERVERS(markDirty(QRect(x, y, image.width(), image.height())));
}

void EditableLayer::putTile(int col, int row, int repeat, const Tile &tile, int sublayer)
{
	Q_ASSERT(d);
	if(col<0 || col >= d->m_xtiles || row<0 || row >= d->m_ytiles)
		return;

	if(sublayer != 0) {
		// LayerAttributes command can be used to set the sublayer's blendmode
		// and opacity before calling putTile, if necessary.
		getEditableSubLayer(sublayer, BlendMode::MODE_NORMAL, 255).putTile(col, row, repeat, tile, 0);
		return;
	}

	int i=row*d->m_xtiles+col;
	const int end = qMin(i+repeat, d->m_tiles.size()-1);
	for(;i<=end;++i) {
		d->m_tiles[i] = tile;
		if(owner && d->isVisible())
			OBSERVERS(markDirty(i));
	}
}

void EditableLayer::fillRect(const QRect &rectangle, const QColor &color, BlendMode::Mode blendmode)
{
	const QRect canvas(0, 0, d->m_width, d->m_height);

	if(rectangle.contains(canvas) && (blendmode==BlendMode::MODE_REPLACE || (blendmode==BlendMode::MODE_NORMAL && color.alpha() == 255))) {
		// Special case: overwrite whole layer
		d->m_tiles.fill(Tile(color));

	} else {
		// The usual case: only a portion of the layer is filled or pixel blending is needed
		QRect rect = rectangle.intersected(canvas);

		uchar mask[Tile::LENGTH];
		if(blendmode==255)
			memset(mask, 0xff, Tile::LENGTH);
		else
			memset(mask, color.alpha(), Tile::LENGTH);

		const int size = Tile::SIZE;
		const int bottom = rect.y() + rect.height();
		const int right = rect.x() + rect.width();

		const int tx0 = rect.x() / size;
		const int tx1 = (right-1) / size;
		const int ty0 = rect.y() / size;
		const int ty1 = (bottom-1) / size;

		bool canIncrOpacity;
		if(blendmode==255 && color.alpha()==0)
			canIncrOpacity = false;
		else
			canIncrOpacity = findBlendMode(blendmode).flags.testFlag(BlendMode::IncrOpacity);

		for(int ty=ty0;ty<=ty1;++ty) {
			for(int tx=tx0;tx<=tx1;++tx) {
				int left = qMax(tx * size, rect.x()) - tx*size;
				int top = qMax(ty * size, rect.y()) - ty*size;
				int w = qMin((tx+1)*size, right) - tx*size - left;
				int h = qMin((ty+1)*size, bottom) - ty*size - top;

				Tile &t = d->m_tiles[ty*d->m_xtiles+tx];
				t.setLastEditedBy(contextId);

				if(!t.isNull() || canIncrOpacity)
					t.composite(blendmode, mask, color, left, top, w, h, 0);
			}
		}
	}

	if(owner && d->isVisible())
		OBSERVERS(markDirty(rectangle));
}

void EditableLayer::putBrushStamp(const BrushStamp &bs, const QColor &color, BlendMode::Mode blendmode)
{
	Q_ASSERT(d);
	const int top=bs.top, left=bs.left;
	const int dia = bs.mask.diameter();
	const int bottom = qMin(top + dia, d->m_height);
	const int right = qMin(left + dia, d->m_width);

	if(left+dia<=0 || top+dia<=0 || left>=d->m_width || top>=d->m_height)
		return;

	// Composite the brush mask onto the layer
	const uchar *values = bs.mask.data();

	// A single dab can (and often does) span multiple tiles.
	int y = top<0?0:top;
	int yb = top<0?-top:0; // y in relation to brush origin
	const int x0 = left<0?0:left;
	const int xb0 = left<0?-left:0;
	while(y<bottom) {
		const int yindex = y / Tile::SIZE;
		const int yt = y - yindex * Tile::SIZE;
		const int hb = yt+dia-yb < Tile::SIZE ? dia-yb : Tile::SIZE-yt;
		int x = x0;
		int xb = xb0; // x in relation to brush origin
		while(x<right) {
			const int xindex = x / Tile::SIZE;
			const int xt = x - xindex * Tile::SIZE;
			const int wb = xt+dia-xb < Tile::SIZE ? dia-xb : Tile::SIZE-xt;
			const int i = d->m_xtiles * yindex + xindex;
			d->m_tiles[i].composite(
					blendmode,
					values + yb * dia + xb,
					color,
					xt, yt,
					wb, hb,
					dia-wb
					);
			d->m_tiles[i].setLastEditedBy(contextId);

			x = (xindex+1) * Tile::SIZE;
			xb = xb + wb;
		}
		y = (yindex+1) * Tile::SIZE;
		yb = yb + hb;
	}

	if(owner && d->isVisible())
		OBSERVERS(markDirty(QRect(left, top, right-left, bottom-top)));
}

/**
 * @brief Merge another layer to this layer
 *
 * Both layer must be the same size
 *
 * @param layer the source layer
 */
void EditableLayer::merge(const Layer *layer)
{
	Q_ASSERT(d);
	Q_ASSERT(layer);
	Q_ASSERT(layer->m_xtiles == d->m_xtiles);
	Q_ASSERT(layer->m_ytiles == d->m_ytiles);

	// Gather a list of non-null source tiles to merge
	QList<int> mergeidx;
	for(int i=0;i<d->m_tiles.size();++i) {
		if(!layer->m_tiles.at(i).isNull())
			mergeidx.append(i);
	}

	// Detach tile vector explicitly to make sure concurrent modifications
	// are all done to the same vector
	d->m_tiles.detach();

	// Merge tiles
	concurrentForEach<int>(mergeidx, [this, layer](int idx) {
		d->m_tiles[idx].merge(layer->m_tiles.at(idx), layer->opacity(), layer->blendmode());
		d->m_tiles[idx].setLastEditedBy(contextId);
	});

	// Merging a layer does not cause an immediate visual change, so we don't
	// mark the area as dirty here.
}

void EditableLayer::makeBlank()
{
	Q_ASSERT(d);
	d->m_tiles.fill(Tile());

	if(owner && d->isVisible())
		OBSERVERS(markDirty());
}

/**
 * This is used to end an indirect stroke.
 * If a sublayer with the given ID does not exist, this function does nothing.
 * @param id
 */
void EditableLayer::mergeSublayer(int id)
{
	Q_ASSERT(d);
	for(Layer *sl : d->m_sublayers) {
		if(sl->id() == id) {
			if(!sl->isHidden()) {
				merge(sl);
				// Set hidden flag directly to avoid markDirty call.
				// The merge should cause no visual change.
				sl->m_info.hidden = true;
			}
			return;
		}
	}
}

void EditableLayer::mergeAllSublayers()
{
	Q_ASSERT(d);
	for(Layer *sl : d->m_sublayers) {
		if(sl->id() > 0) {
			if(!sl->isHidden()) {
				merge(sl);
				// Set hidden flag directly to avoid markDirty call.
				// The merge should cause no visual change.
				sl->m_info.hidden = true;
			}
			return;
		}
	}
}

/**
 * @brief This is used to remove temporary sublayers
 *
 * The layer isn't actually deleted, but is just marked as hidden for recycling.
 * Call optimize() to clean up removed sublayers.
 * @param id
 */
void EditableLayer::removeSublayer(int id)
{
	Q_ASSERT(d);
	for(Layer *sl : d->m_sublayers) {
		if(sl->id() == id) {
			EditableLayer(sl, owner, contextId).setHidden(true);
			return;
		}
	}
}

void EditableLayer::removePreviews()
{
	Q_ASSERT(d);
	for(Layer *sl : d->m_sublayers) {
		if(sl->id() < 0)
			EditableLayer(sl, owner, contextId).setHidden(true);
	}
}

void EditableLayer::markOpaqueDirty(bool forceVisible)
{
	if(!owner || !(forceVisible || d->isVisible()))
		return;

	for(int i=0;i<d->m_tiles.size();++i) {
		if(!d->m_tiles.at(i).isNull())
			OBSERVERS(markDirty(i));
	}
}

}

