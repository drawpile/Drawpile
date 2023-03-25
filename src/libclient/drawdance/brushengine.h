#ifndef DRAWDANCE_BRUSH_ENGINE_H
#define DRAWDANCE_BRUSH_ENGINE_H

#include "libclient/drawdance/message.h"

struct DP_BrushEngine;
struct DP_ClassicBrush;
struct DP_MyPaintBrush;
struct DP_MyPaintSettings;

namespace net {
    class Client;
}

namespace drawdance {

class CanvasState;

class BrushEngine final {
public:
    BrushEngine();
    ~BrushEngine();

    BrushEngine(const BrushEngine &) = delete;
    BrushEngine(BrushEngine &&) = delete;
    BrushEngine &operator=(const BrushEngine &) = delete;
    BrushEngine &operator=(BrushEngine &&) = delete;

    void setClassicBrush(const DP_ClassicBrush &brush, int layerId);

    void setMyPaintBrush(
        const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
        int layerId, bool freehand);

    void flushDabs();

    const drawdance::MessageList &messages() const { return m_messages; }

    void clearMessages() { m_messages.clear(); }

    void beginStroke(unsigned int contextId, bool pushUndoPoint = true);

    void strokeTo(
        float x, float y, float pressure, float xtilt, float ytilt,
        float rotation, long long deltaMsec,
        const drawdance::CanvasState &cs);

    void endStroke(bool pushPenUp = true);

    void addOffset(float x, float y);

    // Flushes dabs and sends accumulated messages to the client.
    void sendMessagesTo(net::Client *client);

private:
    static void pushMessage(void *user, DP_Message *msg);

    drawdance::MessageList m_messages;
    DP_BrushEngine *m_data;
};

}

#endif

