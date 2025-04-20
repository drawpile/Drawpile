// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/canvas_state.h>
#include <dpengine/flood_fill.h>
#include <dpengine/image.h>
#include <dpengine/layer_content.h>
#include <dpengine/layer_group.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpengine/layer_routes.h>
#include <dpengine/snapshots.h>
#include <dpengine/view_mode.h>
#include <dpimpex/load.h>
}
#include "libclient/canvas/blendmodes.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/image.h"
#include "libclient/drawdance/layergroup.h"
#include "libclient/drawdance/layerprops.h"
#include <QObject>

namespace drawdance {

CanvasState CanvasState::null()
{
	return CanvasState{nullptr};
}

CanvasState CanvasState::inc(DP_CanvasState *cs)
{
	return CanvasState{DP_canvas_state_incref_nullable(cs)};
}

CanvasState CanvasState::noinc(DP_CanvasState *cs)
{
	return CanvasState{cs};
}


CanvasState CanvasState::load(
	const QString &path, DP_LoadResult *outResult, DP_SaveImageType *outType)
{
	QByteArray pathBytes = path.toUtf8();
	QByteArray flatImageLayerTitleBytes =
		QObject::tr("Layer %1").arg(1).toUtf8();
	DrawContext dc = DrawContextPool::acquire();
	DP_CanvasState *cs = DP_load(
		dc.get(), pathBytes.constData(), flatImageLayerTitleBytes.constData(),
		loadFlags(), outResult, outType);
	return CanvasState::noinc(cs);
}


CanvasState::CanvasState()
	: CanvasState{nullptr}
{
}

CanvasState::CanvasState(const CanvasState &other)
	: CanvasState{DP_canvas_state_incref_nullable(other.m_data)}
{
}

CanvasState::CanvasState(CanvasState &&other)
	: CanvasState{other.m_data}
{
	other.m_data = nullptr;
}

CanvasState &CanvasState::operator=(const CanvasState &other)
{
	DP_canvas_state_decref_nullable(m_data);
	m_data = DP_canvas_state_incref_nullable(other.m_data);
	return *this;
}

CanvasState &CanvasState::operator=(CanvasState &&other)
{
	DP_canvas_state_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

CanvasState::~CanvasState()
{
	DP_canvas_state_decref_nullable(m_data);
}

DP_CanvasState *CanvasState::get() const
{
	return m_data;
}

DP_CanvasState *CanvasState::getInc() const
{
	return DP_canvas_state_incref_nullable(m_data);
}

bool CanvasState::isNull() const
{
	return !m_data;
}

int CanvasState::width() const
{
	return DP_canvas_state_width(m_data);
}

int CanvasState::height() const
{
	return DP_canvas_state_height(m_data);
}

QSize CanvasState::size() const
{
	return QSize{width(), height()};
}

int CanvasState::offsetX() const
{
	return DP_canvas_state_offset_x(m_data);
}

int CanvasState::offsetY() const
{
	return DP_canvas_state_offset_y(m_data);
}

QPoint CanvasState::offset() const
{
	return QPoint{offsetX(), offsetY()};
}

Tile CanvasState::backgroundTile() const
{
	return Tile::inc(DP_canvas_state_background_tile_noinc(m_data));
}

DocumentMetadata CanvasState::documentMetadata() const
{
	return DocumentMetadata::inc(DP_canvas_state_metadata_noinc(m_data));
}

LayerList CanvasState::layers() const
{
	return LayerList::inc(DP_canvas_state_layers_noinc(m_data));
}

AnnotationList CanvasState::annotations() const
{
	return AnnotationList::inc(DP_canvas_state_annotations_noinc(m_data));
}

Timeline CanvasState::timeline() const
{
	return Timeline::inc(DP_canvas_state_timeline_noinc(m_data));
}

SelectionSet CanvasState::selections() const
{
	return SelectionSet::inc(DP_canvas_state_selections_noinc_nullable(m_data));
}

int CanvasState::frameCount() const
{
	return DP_canvas_state_frame_count(m_data);
}

int CanvasState::framerate() const
{
	return DP_canvas_state_framerate(m_data);
}

bool CanvasState::sameFrame(int frameIndexA, int frameIndexB) const
{
	return DP_canvas_state_same_frame(m_data, frameIndexA, frameIndexB);
}

QSet<int>
CanvasState::getLayersVisibleInTrackFrame(int trackId, int frameIndex) const
{
	QSet<int> layersVisibleInTrackFrame;
	DP_view_mode_get_layers_visible_in_track_frame(
		m_data, trackId, frameIndex, addLayerVisibleInFrame,
		&layersVisibleInTrackFrame);
	return layersVisibleInTrackFrame;
}

QImage CanvasState::toFlatImage(
	bool includeBackground, bool includeSublayers, const QRect *rect,
	const DP_ViewModeFilter *vmf) const
{
	unsigned int flags =
		(includeBackground ? DP_FLAT_IMAGE_INCLUDE_BACKGROUND : 0) |
		(includeSublayers ? DP_FLAT_IMAGE_INCLUDE_SUBLAYERS : 0);
	DP_Rect area;
	if(rect) {
		area =
			DP_rect_make(rect->x(), rect->y(), rect->width(), rect->height());
	}
	DP_Image *img = DP_canvas_state_to_flat_image(
		m_data, flags, rect ? &area : nullptr, vmf);
	return wrapImage(img);
}

QImage CanvasState::layerToFlatImage(int layerId, const QRect &rect) const
{
	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layerId);
	if(lre) {
		if(DP_layer_routes_entry_is_group(lre)) {
			DP_LayerGroup *lg = DP_layer_routes_entry_group(lre, m_data);
			DP_LayerProps *lp = DP_layer_routes_entry_props(lre, m_data);
			return LayerGroup::inc(lg).toImage(LayerProps::inc(lp), rect);
		} else {
			DP_LayerContent *lc = DP_layer_routes_entry_content(lre, m_data);
			return LayerContent::inc(lc).toImage(rect);
		}
	} else {
		return QImage{};
	}
}

QRect CanvasState::layerBounds(int layerId, bool includeSublayers) const
{
	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layerId);
	bool haveBounds = false;
	DP_Rect bounds = {0, 0, 0, 0}; // MSVC emits bogus uninitialized warnings.
	if(lre) {
		if(DP_layer_routes_entry_is_group(lre)) {
			DP_LayerGroup *lg = DP_layer_routes_entry_group(lre, m_data);
			haveBounds = DP_layer_group_bounds(lg, includeSublayers, &bounds);
		} else {
			DP_LayerContent *lc = DP_layer_routes_entry_content(lre, m_data);
			haveBounds = DP_layer_content_bounds(lc, includeSublayers, &bounds);
		}
	}
	return haveBounds ? QRect(
							DP_rect_x(bounds), DP_rect_y(bounds),
							DP_rect_width(bounds), DP_rect_height(bounds))
					  : QRect();
}

bool CanvasState::isBlankIn(
	int layerId, const QRect &rect, const QImage &mask) const
{
	Q_ASSERT(
		mask.isNull() || mask.format() == QImage::Format_ARGB32_Premultiplied);
	QRect bounds = QRect(QPoint(0, 0), size()).intersected(rect);
	if(bounds.isEmpty()) {
		return true;
	}

	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layerId);
	if(!lre || DP_layer_routes_entry_is_group(lre)) {
		return true;
	}

	DP_LayerContent *lc = DP_layer_routes_entry_content(lre, m_data);
	DP_Rect area =
		DP_rect_make(rect.x(), rect.y(), rect.width(), rect.height());
	return mask.isNull()
			   ? DP_layer_content_is_blank_in_bounds(lc, &area)
			   : DP_layer_content_is_blank_in_mask(
					 lc, &area,
					 reinterpret_cast<const DP_Pixel8 *>(mask.constBits()));
}

void CanvasState::toResetImage(net::MessageList &msgs, uint8_t contextId) const
{
	DP_reset_image_build(m_data, contextId, &CanvasState::pushMessage, &msgs);
}

net::Message CanvasState::makeLayerTreeMove(
	uint8_t contextId, int sourceId, int targetId, bool intoGroup,
	bool below) const
{
	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	return net::Message::noinc(DP_layer_routes_layer_tree_move_make(
		lr, m_data, contextId, sourceId, targetId, intoGroup, below));
}

LayerSearchResult CanvasState::searchLayer(int layerId, bool showCensored) const
{
	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layerId);
	DP_LayerProps *lp =
		lre ? DP_layer_routes_entry_props(lre, m_data) : nullptr;
	if(lp && (showCensored || !DP_layer_props_censored(lp))) {
		if(DP_layer_routes_entry_is_group(lre)) {
			return {
				LayerProps::inc(lp),
				LayerGroup::inc(DP_layer_routes_entry_group(lre, m_data))};
		} else {
			return {
				LayerProps::inc(lp),
				LayerContent::inc(DP_layer_routes_entry_content(lre, m_data))};
		}
	} else {
		return {LayerProps::null(), std::monostate()};
	}
}

bool CanvasState::selectionExists(unsigned int contextId, int selectionId) const
{
	return DP_canvas_state_selection_search_noinc(
		m_data, contextId, selectionId);
}

DP_FloodFillResult CanvasState::floodFill(
	unsigned int contextId, int selectionId, int x, int y,
	const QColor &fillColor, double tolerance, int layerId, int sizeLimit,
	int gap, int expand, DP_FloodFillKernel kernel, int featherRadius,
	bool fromEdge, bool continuous, bool includeSublayers, DP_ViewMode viewMode,
	int activeLayerId, int activeFrameIndex, const QAtomicInt &cancel,
	QImage &outImg, int &outX, int &outY) const
{
	DP_UPixelFloat fillPixel = DP_upixel_float_from_color(fillColor.rgba());
	DP_Image *img;
	DP_FloodFillResult result = DP_flood_fill(
		m_data, contextId, selectionId, x, y, fillPixel, tolerance, layerId,
		sizeLimit, gap, expand, kernel, featherRadius, fromEdge, continuous,
		includeSublayers, viewMode, activeLayerId, activeFrameIndex, &img,
		&outX, &outY, shouldCancelFloodFill, const_cast<QAtomicInt *>(&cancel));
	if(result == DP_FLOOD_FILL_SUCCESS) {
		outImg = wrapImage(img);
	}
	return result;
}

DP_FloodFillResult CanvasState::selectionFill(
	unsigned int contextId, int selectionId, const QColor &fillColor,
	int expand, DP_FloodFillKernel kernel, int featherRadius, bool fromEdge,
	const QAtomicInt &cancel, QImage &outImg, int &outX, int &outY) const
{
	DP_UPixelFloat fillPixel = DP_upixel_float_from_color(fillColor.rgba());
	DP_Image *img;
	DP_FloodFillResult result = DP_selection_fill(
		m_data, contextId, selectionId, fillPixel, expand,
		DP_FloodFillKernel(kernel), featherRadius, fromEdge, &img, &outX, &outY,
		shouldCancelFloodFill, const_cast<QAtomicInt *>(&cancel));
	if(result == DP_FLOOD_FILL_SUCCESS) {
		outImg = wrapImage(img);
	}
	return result;
}

unsigned int CanvasState::loadFlags()
{
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
	// Android just kills the application if it uses too much memory for its
	// taste, so it's not safe to use multiple threads to load image data. On
	// Emscripten, "threads" are web workers, which have a lot of overhead. And
	// it may also be running on a mobile device, so the above applies here too.
	return DP_LOAD_FLAG_SINGLE_THREAD;
#else
	return DP_LOAD_FLAG_NONE;
#endif
}

CanvasState::CanvasState(DP_CanvasState *cs)
	: m_data{cs}
{
}

void CanvasState::pushMessage(void *user, DP_Message *msg)
{
	static_cast<net::MessageList *>(user)->append(net::Message::noinc(msg));
}

void CanvasState::addLayerVisibleInFrame(void *user, int layerId, bool visible)
{
	if(visible) {
		QSet<int> *layersVisibleInFrame = static_cast<QSet<int> *>(user);
		layersVisibleInFrame->insert(layerId);
	}
}

bool CanvasState::shouldCancelFloodFill(void *user)
{
	return *static_cast<const QAtomicInt *>(user);
}

}
