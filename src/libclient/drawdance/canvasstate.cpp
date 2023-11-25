// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/canvas_state.h>
#include <dpengine/flood_fill.h>
#include <dpengine/image.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpengine/layer_routes.h>
#include <dpengine/snapshots.h>
#include <dpengine/view_mode.h>
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

void CanvasState::toResetImage(net::MessageList &msgs, uint8_t contextId) const
{
	DP_reset_image_build(m_data, contextId, &CanvasState::pushMessage, &msgs);
}

net::Message CanvasState::makeLayerOrder(
	uint8_t contextId, int sourceId, int targetId, bool below) const
{
	return net::Message::noinc(DP_layer_routes_layer_order_make(
		m_data, contextId, sourceId, targetId, below));
}

net::Message CanvasState::makeLayerTreeMove(
	uint8_t contextId, int sourceId, int targetId, bool intoGroup,
	bool below) const
{
	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	return net::Message::noinc(DP_layer_routes_layer_tree_move_make(
		lr, m_data, contextId, sourceId, targetId, intoGroup, below));
}

LayerContent CanvasState::searchLayerContent(int layerId) const
{
	DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layerId);
	if(lre && !DP_layer_routes_entry_is_group(lre)) {
		return LayerContent::inc(DP_layer_routes_entry_content(lre, m_data));
	} else {
		return LayerContent::null();
	}
}

DP_FloodFillResult CanvasState::floodFill(
	int x, int y, const QColor &fillColor, double tolerance, int layerId,
	int sizeLimit, int gap, int expand, int featherRadius, DP_ViewMode viewMode,
	int activeLayerId, int activeFrameIndex, const QAtomicInt &cancel,
	QImage &outImg, int &outX, int &outY) const
{
	DP_UPixelFloat fillPixel = DP_upixel_float_from_color(fillColor.rgba());
	DP_Image *img;
	DP_FloodFillResult result = DP_flood_fill(
		m_data, x, y, fillPixel, tolerance, layerId, sizeLimit, gap, expand,
		featherRadius, viewMode, activeLayerId, activeFrameIndex, &img, &outX,
		&outY, shouldCancelFloodFill, const_cast<QAtomicInt *>(&cancel));
	if(result == DP_FLOOD_FILL_SUCCESS) {
		outImg = wrapImage(img);
	}
	return result;
}

drawdance::CanvasState CanvasState::makeBackwardCompatible() const
{
	if(!m_data) {
		return null();
	}

	DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(m_data);
	DP_TransientLayerList *tll =
		DP_transient_canvas_state_transient_layers(tcs, 0);
	DP_TransientLayerPropsList *tlpl =
		DP_transient_canvas_state_transient_layer_props(tcs, 0);
	int count = DP_transient_layer_list_count(tll);
	for(int i = 0; i < count; ++i) {
		DP_LayerProps *lp = DP_transient_layer_props_list_at_noinc(tlpl, i);
		bool isGroup = DP_layer_props_children_noinc(lp);
		bool isModeIncompatible = !canvas::blendmode::isBackwardCompatibleMode(
			DP_BlendMode(DP_layer_props_blend_mode(lp)));

		if(isGroup) {
			DP_transient_layer_list_merge_at(tll, lp, i);
			DP_transient_layer_props_list_merge_at(tlpl, i);
		}

		if(isModeIncompatible) {
			DP_TransientLayerProps *tlp =
				DP_transient_layer_props_list_transient_at_noinc(tlpl, i);
			DP_transient_layer_props_blend_mode_set(tlp, DP_BLEND_MODE_NORMAL);
		}
	}

	// Other stuff like the timeline or document metadata will just get dropped.
	return drawdance::CanvasState::noinc(
		DP_transient_canvas_state_persist(tcs));
}

unsigned int CanvasState::loadFlags()
{
#ifdef Q_OS_ANDROID
	// Android just kills the application if it uses too much memory for its
	// taste, so it's not safe to use multiple threads to load image data.
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
