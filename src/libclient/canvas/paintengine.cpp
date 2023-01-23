/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#include "libclient/canvas/paintengine.h"
#include "rustpile/rustpile.h"

#include <QPainter>

namespace {
static const int TILE_SIZE = 64; // FIXME

static void updateCacheTile(void *p, int x, int y, const uchar *pixels)
{
	((QPainter*)p)->drawImage(
			x,
			y,
			QImage(pixels,
				TILE_SIZE, TILE_SIZE,
				QImage::Format_ARGB32_Premultiplied
			)
	);
}

}

namespace canvas {

// Callback: notify the view that an area on the canvas is in need of refresh.
// Refreshing is lazy: if that area is not visible at the moment, it doesn't
// need to be repainted now.
void paintEngineAreaChanged(void *pe, rustpile::Rectangle area)
{
	emit reinterpret_cast<PaintEngine*>(pe)->areaChanged(QRect(area.x, area.y, area.w, area.h));
}

// Callback: notify the view that the canvas size has changed.
void paintEngineResized(void *pe, int xoffset, int yoffset, rustpile::Size oldSize)
{
	emit reinterpret_cast<PaintEngine*>(pe)->resized(xoffset, yoffset, QSize{oldSize.width, oldSize.height});
}

void paintEngineLayersChanged(void *pe, const rustpile::LayerInfo *layerInfos, uintptr_t count)
{
	QVector<LayerListItem> layers;
	layers.reserve(count);

	for(uintptr_t i=0;i<count;++i) {
		const rustpile::LayerInfo &li = layerInfos[i];
		layers << LayerListItem {
			uint16_t(li.id), // only internal (non-visible) layers have IDs outside the u16 range
			uint16_t(li.frame_id),
			QString::fromUtf8(reinterpret_cast<const char*>(li.title), li.titlelen),
			li.opacity,
			li.blendmode,
			li.hidden,
			li.censored,
			li.isolated,
			li.group,
			li.children,
			li.rel_index,
			li.left,
			li.right
		};
	}

	emit reinterpret_cast<PaintEngine*>(pe)->layersChanged(layers);
}

void paintEngineAnnotationsChanged(void *pe, rustpile::Annotations *annotations)
{
	// Note: rustpile::Annotations is thread safe
	emit reinterpret_cast<PaintEngine*>(pe)->annotationsChanged(annotations);
}

void paintEngineMetadataChanged(void *pe)
{
	emit reinterpret_cast<PaintEngine*>(pe)->metadataChanged();
}

void paintEngineTimelineChanged(void *pe)
{
	emit reinterpret_cast<PaintEngine*>(pe)->timelineChanged();
}

void paintEngineFrameVisbilityChanged(void *pe, const rustpile::Frame *frame, bool frameMode)
{
	QVector<int> layers;
	if(frameMode) {
		for(unsigned long i=0;i<sizeof(rustpile::Frame)/sizeof(rustpile::LayerID);++i) {
			if((*frame)[i] != 0)
				layers << (*frame)[i];
			else
				break;
		}
	}
	emit reinterpret_cast<PaintEngine*>(pe)->frameVisibilityChanged(layers, frameMode);
}

void paintEngineCursors(void *pe, uint8_t user, uint16_t layer, int32_t x, int32_t y)
{
	// This may be emitted from either the main thread or the paint engine thread
	emit reinterpret_cast<PaintEngine*>(pe)->cursorMoved(user, layer, x, y);
}

void paintEnginePlayback(void *pe, int64_t pos, uint32_t interval)
{
	emit reinterpret_cast<PaintEngine*>(pe)->playbackAt(pos, interval);
}

void paintEngineCatchup(void *pe, uint32_t progress)
{
	emit reinterpret_cast<PaintEngine*>(pe)->caughtUpTo(progress);
}

PaintEngine::PaintEngine(QObject *parent)
	: QObject(parent), m_pe(nullptr)
{
	reset();
}

PaintEngine::~PaintEngine()
{
	rustpile::paintengine_free(m_pe);
}

void PaintEngine::reset()
{
	rustpile::paintengine_free(m_pe);
	m_pe = rustpile::paintengine_new(
		this,
		paintEngineAreaChanged,
		paintEngineResized,
		paintEngineLayersChanged,
		paintEngineAnnotationsChanged,
		paintEngineCursors,
		paintEnginePlayback,
		paintEngineCatchup,
		paintEngineMetadataChanged,
		paintEngineTimelineChanged,
		paintEngineFrameVisbilityChanged
	);

	m_cache = QPixmap();
}

void PaintEngine::receiveMessages(bool local, const net::Envelope &msgs)
{
	if(!rustpile::paintengine_receive_messages(m_pe, local, msgs.data(), msgs.length()))
		emit enginePanicked();
}

void PaintEngine::enqueueCatchupProgress(int progress)
{
	if(!rustpile::paintengine_enqueue_catchup(m_pe, progress))
		emit enginePanicked();
}

void PaintEngine::cleanup()
{
	rustpile::paintengine_cleanup(m_pe);
}

QColor PaintEngine::backgroundColor() const
{
	const auto c = rustpile::paintengine_background_color(m_pe);
	return QColor::fromRgbF(c.r, c.g, c.b, c.a);
}

uint16_t PaintEngine::findAvailableAnnotationId(uint8_t forUser) const
{
	return rustpile::paintengine_get_available_annotation_id(m_pe, forUser);
}

rustpile::AnnotationAt PaintEngine::getAnnotationAt(int x, int y, int expand) const
{
	return rustpile::paintengine_get_annotation_at(m_pe, x, y, expand);
}

bool PaintEngine::needsOpenRaster() const
{
	return !rustpile::paintengine_is_simple(m_pe);
}

void PaintEngine::setViewMode(rustpile::LayerViewMode mode, bool censor)
{
	rustpile::paintengine_set_view_mode(m_pe, mode, censor);
}

bool PaintEngine::isCensored() const
{
	return rustpile::paintengine_is_censored(m_pe);
}

void PaintEngine::setOnionskinOptions(int skinsBelow, int skinsAbove, bool tint)
{
	rustpile::paintengine_set_onionskin_opts(m_pe, skinsBelow, skinsAbove, tint);
}

void PaintEngine::setViewLayer(int id)
{
	rustpile::paintengine_set_active_layer(m_pe, id);
}

void PaintEngine::setViewFrame(int frame)
{
	rustpile::paintengine_set_active_frame(m_pe, frame);
}

const QPixmap& PaintEngine::getPixmap(const QRect &refreshArea)
{
	const auto size = this->size();
	if(size.isEmpty())
		return m_cache;

	if(m_cache.isNull() || m_cache.size() != size) {
		m_cache = QPixmap(size);
		m_cache.fill();
	}

	const rustpile::Rectangle r {
		refreshArea.x(),
		refreshArea.y(),
		refreshArea.width(),
		refreshArea.height()
	};

	QPainter painter(&m_cache);
	painter.setCompositionMode(QPainter::CompositionMode_Source);

	rustpile::paintengine_paint_changes(m_pe, &painter, r, &updateCacheTile);

	return m_cache;
}

int PaintEngine::frameCount() const
{
	return rustpile::paintengine_get_frame_count(m_pe);
}

QImage PaintEngine::getLayerImage(int id, const QRect &rect) const
{
	rustpile::Rectangle r;
	if(rect.isEmpty()) {
		r = rustpile::paintengine_get_layer_bounds(m_pe, id);
		if(r.w <= 0)
			return QImage();

	} else {
		r = {rect.x(), rect.y(), rect.width(), rect.height()};
	}

	QImage img(r.w, r.h, QImage::Format_ARGB32_Premultiplied);
	img.fill(0);

	if(!rustpile::paintengine_get_layer_content(m_pe, id, r, img.bits())) {
		return QImage();
	}

	return img;
}

QImage PaintEngine::getFrameImage(int index, const QRect &rect) const
{
	rustpile::Rectangle r;
	if(rect.isEmpty()) {
		const auto size = rustpile::paintengine_canvas_size(m_pe);
		r = {0, 0, size.width, size.height};
	} else {
		r = {rect.x(), rect.y(), rect.width(), rect.height()};
	}

	QImage img(r.w, r.h, QImage::Format_ARGB32_Premultiplied);

	if(!rustpile::paintengine_get_frame_content(m_pe, index, r, img.bits())) {
		return QImage();
	}

	return img;
}

QSize PaintEngine::size() const
{
	const auto size = rustpile::paintengine_canvas_size(m_pe);
	return QSize(size.width, size.height);
}

}
