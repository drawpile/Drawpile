// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_MESSAGE_H
#define DRAWDANCE_MESSAGE_H

extern "C" {
#include <dpmsg/message.h>
}

#include <QRect>
#include <QVector>

struct DP_Message;

class QByteArray;
class QColor;
class QImage;
class QString;

namespace drawdance {

using MessageList = QVector<class Message>;

class Message final {
public:
    static Message null();
    static Message inc(DP_Message *cs);
    static Message noinc(DP_Message *cs);
    static Message deserialize(const unsigned char *buf, size_t bufsize);

    static DP_Message **asRawMessages(const drawdance::Message *msgs);

    static Message makeAnnotationDelete(uint8_t contextId, uint16_t id);
    static Message makeAnnotationEdit(uint8_t contextId, uint16_t id, uint32_t bg, uint8_t flags, uint8_t border, const QString &text);
    static Message makeCanvasBackground(uint8_t contextId, const QColor &color);
    static Message makeCanvasResize(uint8_t contextId, int32_t top, int32_t right, int32_t bottom, int32_t left);
    static Message makeChat(uint8_t contextId, uint8_t tflags, uint8_t oflags, const QString &message);
    static Message makeDefaultLayer(uint8_t contextId, uint16_t id);
    static Message makeFillRect(uint8_t contextId, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const QColor &color);
    static Message makeInternalCatchup(uint8_t contextId, int progress);
    static Message makeInternalCleanup(uint8_t contextId);
    static Message makeInternalReset(uint8_t contextId);
    static Message makeInternalSnapshot(uint8_t contextId);
    static Message makeLayerAttributes(uint8_t contextId, uint16_t id, uint8_t sublayer, uint8_t flags, uint8_t opacity, uint8_t blend);
    static Message makeLayerAcl(uint8_t contextId, uint16_t id, uint8_t flags, const QVector<uint8_t> &exclusive);
    static Message makeLayerCreate(uint8_t contextId, uint16_t id, uint16_t source, uint16_t target, uint32_t fill, uint8_t flags, const QString &name);
    static Message makeLayerDelete(uint8_t contextId, uint16_t id, uint16_t mergeTo);
    static Message makeLayerRetitle(uint8_t contextId, uint16_t id, const QString &title);
    static Message makeMoveRect(uint8_t contextId, uint16_t layer, int32_t sx, int32_t sy, int32_t tx, int32_t ty, int32_t w, int32_t h, const QImage &mask);
    static Message makePrivateChat(uint8_t contextId, uint8_t target, uint8_t oflags, const QString &message);
    static Message makePutImage(uint8_t contextId, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const QByteArray &compressedImage);
    static Message makeSessionOwner(uint8_t contextId, const QVector<uint8_t> &users);
    static Message makeSetMetadataInt(uint8_t contextId, uint8_t field, int32_t value);
    static Message makeSetTimelineFrame(uint8_t contextId, uint16_t frame, bool insert, const QVector<uint16_t> &layerIds);
    static Message makeRemoveTimelineFrame(uint8_t contextId, uint16_t frame);
    static Message makeTrustedUsers(uint8_t contextId, const QVector<uint8_t> &users);
    static Message makeUndo(uint8_t contextId, uint8_t overrideUser, bool redo);
    static Message makeUndoPoint(uint8_t contextId);
    static Message makeUserAcl(uint8_t contextId, const QVector<uint8_t> &users);

    // Fills given message list with put image messages, potentially cropping
    // the image if the given coordinates are negative and splitting it into
    // multiple messages if it doesn't fit into a single one.
    static void makePutImages(MessageList &msgs, uint8_t contextId, uint16_t layer, uint8_t mode, int x, int y, const QImage &image);

    Message();
    Message(const Message &other);
    Message(Message &&other);

    Message &operator=(const Message &other);
    Message &operator=(Message &&other);

    ~Message();

    DP_Message *get() const;

    bool isNull() const;

    DP_MessageType type() const;

    unsigned int contextId() const;

    size_t length() const;

    DP_MsgServerCommand *toServerCommand() const;

    bool serialize(QByteArray &buffer) const;

private:
    explicit Message(DP_Message *cs);

    static void makePutImagesRecursive(MessageList &msgs, uint8_t contextId, uint16_t layer, uint8_t mode, int x, int y, const QImage &image, const QRect &bounds, int estimatedSize = 0);

    static unsigned char *getDeserializeBuffer(void *user, size_t size);

    static void setUchars(size_t size, unsigned char *out, void *user);
    static void setUint8s(int count, uint8_t *out, void *user);
    static void setUint16s(int count, uint16_t *out, void *user);

    DP_Message *m_data;
};

}

#endif
