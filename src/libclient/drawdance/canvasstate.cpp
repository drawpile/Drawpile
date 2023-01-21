extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/canvas_state.h>
#include <dpengine/flood_fill.h>
#include <dpengine/image.h>
#include <dpengine/layer_routes.h>
#include <dpengine/snapshots.h>
}

#include "canvasstate.h"
#include "drawcontextpool.h"
#include "image.h"
#include "layergroup.h"
#include "layerprops.h"

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


CanvasState CanvasState::load(const QString &path, DP_LoadResult *outResult)
{
    QByteArray pathBytes = path.toUtf8();
    QByteArray flatImageLayerTitleBytes = QObject::tr("Layer %1").arg(1).toUtf8();
    DrawContext dc = DrawContextPool::acquire();
    DP_CanvasState *cs = DP_load(
        dc.get(), pathBytes.constData(), flatImageLayerTitleBytes.constData(), outResult);
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

QImage CanvasState::toFlatImage(
    bool includeBackground, bool includeSublayers,
    const QRect *rect, const DP_ViewModeFilter *vmf) const
{
    unsigned int flags =
        (includeBackground ? DP_FLAT_IMAGE_INCLUDE_BACKGROUND : 0) |
        (includeSublayers ? DP_FLAT_IMAGE_INCLUDE_SUBLAYERS : 0);
    DP_Rect area;
    if(rect) {
        area = DP_rect_make(rect->x(), rect->y(), rect->width(), rect->height());
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

void CanvasState::toResetImage(MessageList &msgs, uint8_t contextId) const
{
    DP_reset_image_build(m_data, contextId, &CanvasState::pushMessage, &msgs);
}

drawdance::Message CanvasState::makeLayerOrder(
    uint8_t contextId, int sourceId, int targetId, bool intoGroup, bool below) const
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(m_data);
	return drawdance::Message::noinc(DP_layer_routes_layer_order_make(
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

int CanvasState::pickLayer(int x, int y) const
{
    return DP_canvas_state_pick_layer(m_data, x, y);
}

unsigned int CanvasState::pickContextId(int x, int y) const
{
    return DP_canvas_state_pick_context_id(m_data, x, y);
}


DP_FloodFillResult CanvasState::floodFill(
    int x, int y, const QColor &fillColor, double tolerance, int layerId,
    bool sampleMerged, int sizeLimit, int expand, QImage &outImg, int &outX,
    int &outY) const
{
	DP_Pixel8 fillColorPixel = {fillColor.rgba()};
	DP_Image *img;
    DP_FloodFillResult result = DP_flood_fill(
		m_data, x, y, fillColorPixel, tolerance, layerId, sampleMerged,
        sizeLimit, expand, &img, &outX, &outY);
    if(result == DP_FLOOD_FILL_SUCCESS) {
        outImg = wrapImage(img);
    }
    return result;
}

CanvasState::CanvasState(DP_CanvasState *cs)
    : m_data{cs}
{
}

void CanvasState::pushMessage(void *user, DP_Message *msg)
{
    static_cast<MessageList *>(user)->append(drawdance::Message::noinc(msg));
}

}
