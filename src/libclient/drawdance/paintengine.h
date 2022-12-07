#ifndef DRAWDANCE_PAINTENGINE_H
#define DRAWDANCE_PAINTENGINE_H

extern "C" {
#include <dpengine/paint_engine.h>
}

#include "canvasstate.h"
#include "drawcontextpool.h"
#include <QtGlobal>

struct DP_PaintEngine;

namespace drawdance {

class AclState;
class SnapshotQueue;

enum RecordStartResult {
    RECORD_START_SUCCESS,
    RECORD_START_UNKNOWN_FORMAT,
    RECORD_START_OPEN_ERROR,
};

class PaintEngine {
public:
    PaintEngine(AclState &acls, SnapshotQueue &sq,
        const CanvasState &canvasState = CanvasState::null());

    ~PaintEngine();

    PaintEngine(const PaintEngine &) = delete;
    PaintEngine(PaintEngine &&) = delete;
    PaintEngine &operator=(const PaintEngine &) = delete;
    PaintEngine &operator=(PaintEngine &&) = delete;

    DP_PaintEngine *get();

    void reset(AclState &acls, SnapshotQueue &sq,
        const CanvasState &canvasState = CanvasState::null());

    int renderThreadCount() const;

    LayerContent renderContent() const;

    void setLocalDrawingInProgress(bool localDrawingInProgress);

    void setActiveLayerId(int layerId);

    void setActiveFrameIndex(int frameIndex);

    void setViewMode(DP_ViewMode vm);

    void setOnionSkins(const DP_OnionSkins *oss);

    bool revealCensored() const;
    void setRevealCensored(bool revealCensored);

    void setInspectContextId(unsigned int contextId);

    void setLayerVisibility(int layerId, bool hidden);

    RecordStartResult startRecorder(const QString &path);
    bool stopRecorder();
    bool recorderIsRecording() const;

    void previewCut(int layerId, const QRect &bounds, const QImage &mask);
    void previewDabs(int layerId, int count, const drawdance::Message *msgs);
    void clearPreview();

    CanvasState canvasState() const;

private:
    DrawContext m_paintDc;
    DrawContext m_previewDc;
    DP_PaintEngine *m_data;

    static long long getTimeMs(void *);
};

}

#endif
