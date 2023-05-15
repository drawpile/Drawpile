// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_CANVASSTATE_H
#define DRAWDANCE_CANVASSTATE_H

extern "C" {
#include <dpengine/flood_fill.h>
#include <dpengine/load.h>
}

#include "libclient/drawdance/annotationlist.h"
#include "libclient/drawdance/documentmetadata.h"
#include "libclient/drawdance/layercontent.h"
#include "libclient/drawdance/layerlist.h"
#include "libclient/drawdance/message.h"
#include "libclient/drawdance/tile.h"
#include "libclient/drawdance/timeline.h"
#include <QImage>
#include <QMetaType>
#include <QSet>
#include <QSize>

struct DP_CanvasState;
struct DP_ViewModeFilter;

namespace drawdance {

class CanvasState final {
public:
    static CanvasState null();
    static CanvasState inc(DP_CanvasState *cs);
    static CanvasState noinc(DP_CanvasState *cs);

    static CanvasState load(const QString &path, DP_LoadResult *outResult = nullptr);

    CanvasState();
    CanvasState(const CanvasState &other);
    CanvasState(CanvasState &&other);

    CanvasState &operator=(const CanvasState &other);
    CanvasState &operator=(CanvasState &&other);

    ~CanvasState();

    DP_CanvasState *get() const;

    bool isNull() const;

    int width() const;
    int height() const;
    QSize size() const;

    Tile backgroundTile() const;
    DocumentMetadata documentMetadata() const;
    LayerList layers() const;
    AnnotationList annotations() const;
    Timeline timeline() const;

    int frameCount() const;

    bool sameFrame(int frameIndexA, int frameIndexB) const;

    QSet<int> getLayersVisibleInFrame(int frameIndex) const;

    QImage toFlatImage(
        bool includeBackground = true, bool includeSublayers = true,
        const QRect *rect = nullptr, const DP_ViewModeFilter *vmf = nullptr) const;

    QImage layerToFlatImage(int layerId, const QRect &rect) const;

    void toResetImage(MessageList &msgs, uint8_t contextId) const;

    drawdance::Message makeLayerOrder(
        uint8_t contextId, int sourceId, int targetId, bool below) const;

    drawdance::Message makeLayerTreeMove(
        uint8_t contextId, int sourceId, int targetId, bool intoGroup, bool below) const;

    LayerContent searchLayerContent(int layerId) const;

    int pickLayer(int x, int y) const;

    unsigned int pickContextId(int x, int y) const;

    DP_FloodFillResult floodFill(
        int x, int y, const QColor &fillColor, double tolerance, int layerId,
        int sizeLimit, int gap, int expand, int featherRadius,
        const QAtomicInt &cancel, QImage &outImg, int &outX, int &outY) const;

    drawdance::CanvasState makeBackwardCompatible() const;

private:
    explicit CanvasState(DP_CanvasState *cs);

    static void pushMessage(void *user, DP_Message *msg);

    static void addLayerVisibleInFrame(void *user, int layerId, bool visible);

    static bool shouldCancelFloodFill(void *user);

    DP_CanvasState *m_data;
};

}

Q_DECLARE_METATYPE(drawdance::CanvasState)

#endif
