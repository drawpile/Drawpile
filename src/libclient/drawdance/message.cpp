// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpmsg/msg_internal.h>
}

#include "libclient/drawdance/message.h"
#include "libshared/util/qtcompat.h"

#include <QByteArray>
#include <QImage>
#include <QJsonDocument>
#include <QString>
#include <QtEndian>

namespace drawdance {

Message Message::null()
{
    return Message{nullptr};
}

Message Message::inc(DP_Message *msg)
{
    return Message{DP_message_incref_nullable(msg)};
}

Message Message::noinc(DP_Message *msg)
{
    return Message{msg};
}

Message Message::deserialize(const unsigned char *buf, size_t bufsize)
{
    return Message::noinc(DP_message_deserialize(buf, bufsize));
}


DP_Message **Message::asRawMessages(const drawdance::Message *msgs)
{
	// We want to do a moderately evil reinterpret cast of a drawdance::Message
	// to its underlying pointer. Let's make sure that it's a valid thing to do.
	// Make sure it's a standard layout class, because only for those it's legal
	// to cast them to their first member.
	static_assert(std::is_standard_layout<drawdance::Message>::value,
		"drawdance::Message is standard layout for reinterpretation to DP_Message");
	// And then ensure that there's only the pointer member.
	static_assert(sizeof(drawdance::Message) == sizeof(DP_Message *),
		"drawdance::Message has the same size as a DP_Message pointer");
	// Alright, that means this cast, despite looking terrifying, is legal. The
	// const can be cast away safely too because the underlying pointer isn't.
	return reinterpret_cast<DP_Message **>(const_cast<drawdance::Message *>(msgs));
}


Message Message::makeAnnotationDelete(uint8_t contextId, uint16_t id)
{
    return Message{DP_msg_annotation_delete_new(contextId, id)};
}

Message Message::makeAnnotationEdit(uint8_t contextId, uint16_t id, uint32_t bg, uint8_t flags, uint8_t border, const QString &text)
{
    QByteArray bytes = text.toUtf8();
    return Message{DP_msg_annotation_edit_new(
        contextId, id, bg, flags, border, bytes.constData(), bytes.length())};
}

Message Message::makeCanvasBackground(uint8_t contextId, const QColor &color)
{
	uint32_t c = qToBigEndian(color.rgba());
    return Message{DP_msg_canvas_background_new(contextId, setUchars, sizeof(c), &c)};
}

Message Message::makeCanvasResize(uint8_t contextId, int32_t top, int32_t right, int32_t bottom, int32_t left)
{
    return Message{DP_msg_canvas_resize_new(contextId, top, right, bottom, left)};
}

Message Message::makeChat(uint8_t contextId, uint8_t tflags, uint8_t oflags, const QString &message)
{
    QByteArray bytes = message.toUtf8();
    return Message{DP_msg_chat_new(contextId, tflags, oflags, bytes.constData(), bytes.length())};
}

Message Message::makeDefaultLayer(uint8_t contextId, uint16_t id)
{
    return Message{DP_msg_default_layer_new(contextId, id)};
}

Message Message::makeFillRect(uint8_t contextId, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const QColor &color)
{
    return Message{DP_msg_fill_rect_new(contextId, layer, mode, x, y, w, h, color.rgba())};
}

Message Message::makeInternalCatchup(uint8_t contextId, int progress)
{
    return Message{DP_msg_internal_catchup_new(contextId, progress)};
}

Message Message::makeInternalCleanup(uint8_t contextId)
{
    return Message{DP_msg_internal_cleanup_new(contextId)};
}

Message Message::makeInternalReset(uint8_t contextId)
{
    return Message{DP_msg_internal_reset_new(contextId)};
}

Message Message::makeInternalSnapshot(uint8_t contextId)
{
    return Message{DP_msg_internal_snapshot_new(contextId)};
}

Message Message::makeLayerAttributes(uint8_t contextId, uint16_t id, uint8_t sublayer, uint8_t flags, uint8_t opacity, uint8_t blend)
{
    return Message{DP_msg_layer_attributes_new(contextId, id, sublayer, flags, opacity, blend)};
}

Message Message::makeLayerAcl(uint8_t contextId, uint16_t id, uint8_t flags, const QVector<uint8_t> &exclusive)
{
    return Message{DP_msg_layer_acl_new(
        contextId, id, flags, setUint8s, exclusive.count(), const_cast<uint8_t *>(exclusive.constData()))};
}

Message Message::makeLayerCreate(uint8_t contextId, uint16_t id, uint16_t source, uint16_t target, uint32_t fill, uint8_t flags, const QString &name)
{
    QByteArray bytes = name.toUtf8();
    return Message{DP_msg_layer_create_new(
        contextId, id, source, target, fill, flags, bytes.constData(), bytes.length())};
}

Message Message::makeLayerDelete(uint8_t contextId, uint16_t id, uint16_t mergeTo)
{
    return Message{DP_msg_layer_delete_new(contextId, id, mergeTo)};
}

Message Message::makeLayerRetitle(uint8_t contextId, uint16_t id, const QString &title)
{
    QByteArray bytes = title.toUtf8();
    return Message{DP_msg_layer_retitle_new(contextId, id, bytes.constData(), bytes.length())};
}

Message Message::makeMoveRect(uint8_t contextId, uint16_t layer, uint16_t source, int32_t sx, int32_t sy, int32_t tx, int32_t ty, int32_t w, int32_t h, const QImage &mask)
{
    QByteArray compressed = mask.isNull() ? QByteArray{} : compressAlphaMask(mask);
    if(compressed.size() <= DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_MOVE_RECT_STATIC_LENGTH) {
        return Message{DP_msg_move_rect_new(contextId, layer, source, sx, sy, tx, ty, w, h, &Message::setUchars, compressed.size(), compressed.data())};
    } else {
        return Message::null();
    }
}

Message Message::makeMoveRegion(uint8_t contextId, uint16_t layer, uint16_t source, int32_t bx, int32_t by, int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode, const QImage &mask)
{
    QByteArray compressed = mask.isNull() ? QByteArray{} : compressAlphaMask(mask);
    if(compressed.size() <= DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_MOVE_REGION_STATIC_LENGTH) {
        return Message{DP_msg_move_region_new(contextId, layer, source, bx, by, bw, bh, x1, y1, x2, y2, x3, y3, x4, y4, mode, &Message::setUchars, compressed.size(), compressed.data())};
    } else {
        return Message::null();
    }
}

Message Message::makePrivateChat(uint8_t contextId, uint8_t target, uint8_t oflags, const QString &message)
{
    QByteArray bytes = message.toUtf8();
    return Message{DP_msg_private_chat_new(contextId, target, oflags, bytes.constData(), bytes.length())};
}

Message Message::makePutImage(uint8_t contextId, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const QByteArray &compressedImage)
{
    return Message{DP_msg_put_image_new(
        contextId, layer, mode, x, y, w, h, setUchars, compressedImage.size(), const_cast<char *>(compressedImage.data()))};
}

Message Message::makeSessionOwner(uint8_t contextId, const QVector<uint8_t> &users)
{
    return Message{DP_msg_session_owner_new(
        contextId, setUint8s, users.count(), const_cast<uint8_t *>(users.constData()))};
}

Message Message::makeSetMetadataInt(uint8_t contextId, uint8_t field, int32_t value)
{
    return Message{DP_msg_set_metadata_int_new(contextId, field, value)};
}

Message Message::makeTrustedUsers(uint8_t contextId, const QVector<uint8_t> &users)
{
    return Message{DP_msg_trusted_users_new(
        contextId, setUint8s, users.count(), const_cast<uint8_t *>(users.constData()))};
}

Message Message::makeUndo(uint8_t contextId, uint8_t overrideUser, bool redo)
{
    return Message{DP_msg_undo_new(contextId, overrideUser, redo)};
}

Message Message::makeSetTimelineFrame(uint8_t contextId, uint16_t frame, bool insert, const QVector<uint16_t> &layerIds)
{
    return Message{DP_msg_set_timeline_frame_new(contextId, frame, insert, setUint16s, layerIds.count(), const_cast<uint16_t *>(layerIds.constData()))};
}

Message Message::makeRemoveTimelineFrame(uint8_t contextId, uint16_t frame)
{
    return Message{DP_msg_remove_timeline_frame_new(contextId, frame)};
}

Message Message::makeUndoDepth(uint8_t contextId, uint8_t depth)
{
    return Message{DP_msg_undo_depth_new(contextId, depth)};
}

Message Message::makeUndoPoint(uint8_t contextId)
{
    return Message{DP_msg_undo_point_new(contextId)};
}

Message Message::makeUserAcl(uint8_t contextId, const QVector<uint8_t> &users)
{
    return Message{DP_msg_user_acl_new(
        contextId, setUint8s, users.count(), const_cast<uint8_t *>(users.constData()))};
}

Message Message::makeUserInfo(uint8_t contextId, uint8_t recipient, const QJsonDocument &msg)
{
    QByteArray msgBytes = msg.toJson(QJsonDocument::Compact);
    return Message{DP_msg_data_new(
        contextId, DP_MSG_DATA_TYPE_USER_INFO, recipient, setUchars,
        msgBytes.length(), const_cast<char *>(msgBytes.constData()))};
}


void Message::makePutImages(MessageList &msgs, uint8_t contextId, uint16_t layer, uint8_t mode, int x, int y, const QImage &image)
{
    // If the image is totally outside of the canvas, there's nothing to put.
    if(x >= -image.width() && y >= -image.height()) {
        QImage converted = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if(x < 0 || y < 0) {
            // Crop image, since the protocol doesn't do negative coordinates.
            int xoffset = x < 0 ? -x : 0;
            int yoffset = y < 0 ? -y : 0;
            QImage cropped = converted.copy(xoffset, yoffset, image.width() - xoffset, image.height() - yoffset);
            makePutImagesRecursive(msgs, contextId, layer, mode, x + xoffset, y + yoffset, cropped, cropped.rect());
        } else {
            makePutImagesRecursive(msgs, contextId, layer, mode, x, y, converted, converted.rect());
        }
    }
}


Message::Message()
    : Message(nullptr)
{
}

Message::Message(const Message &other)
    : Message{DP_message_incref_nullable(other.m_data)}
{
}

Message::Message(Message &&other)
    : Message{other.m_data}
{
    other.m_data = nullptr;
}

Message &Message::operator=(const Message &other)
{
    DP_message_decref_nullable(m_data);
    m_data = DP_message_incref_nullable(other.m_data);
    return *this;
}

Message &Message::operator=(Message &&other)
{
    DP_message_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

Message::~Message()
{
    DP_message_decref_nullable(m_data);
}

DP_Message *Message::get() const
{
    return m_data;
}

bool Message::isNull() const
{
    return !m_data;
}

DP_MessageType Message::type() const
{
    return DP_message_type(m_data);
}

unsigned int Message::contextId() const
{
    return DP_message_context_id(m_data);
}

size_t Message::length() const
{
    return DP_message_length(m_data);
}

DP_MsgServerCommand *Message::toServerCommand() const
{
    return DP_msg_server_command_cast(m_data);
}

DP_MsgData *Message::toData() const
{
    return DP_msg_data_cast(m_data);
}

bool Message::serialize(QByteArray &buffer) const
{
    return DP_message_serialize(m_data, true, getDeserializeBuffer, &buffer) != 0;
}

Message::Message(DP_Message *msg)
    : m_data{msg}
{
}

QByteArray Message::compressAlphaMask(const QImage &mask)
{
    Q_ASSERT(mask.format() == QImage::Format_ARGB32_Premultiplied);
    int width = mask.width();
    int height = mask.height();
    QByteArray alphaMask;
    alphaMask.reserve(width * height);
    for(int y = 0; y < height; ++y) {
        const uchar *scanLine = mask.scanLine(y);
        for(int x = 0; x < width; ++x) {
            alphaMask.append(scanLine[x * 4 + 3]);
        }
    }
    return qCompress(alphaMask);
}

void Message::makePutImagesRecursive(MessageList &msgs, uint8_t contextId, uint16_t layer, uint8_t mode, int x, int y, const QImage &image, const QRect &bounds, int estimatedSize)
{
    int w = bounds.width();
    int h = bounds.height();
    if (w > 0 && h > 0) {
        int maxSize = 0xffff - 19;
        int compressedSize;
        // If our estimated size looks good, try compressing. Otherwise assume
        // that the image is too big to fit into a message and split it up.
        if(estimatedSize < maxSize) {
            QImage subImage = bounds == image.rect() ? image : image.copy(bounds);
            QByteArray compressed = qCompress(subImage.constBits(), subImage.sizeInBytes());
            compressedSize = compressed.size();
            if(compressedSize <= maxSize) {
                msgs.append(makePutImage(contextId, layer, mode, x, y, w, h, compressed));
                return;
            }
        } else {
            compressedSize = estimatedSize;
        }
        // (Probably) too big to fit in a message, slice in half along the longest axis.
        int estimatedSliceSize = compressedSize / 2;
        if (w > h) {
            int sx1 = w / 2;
            int sx2 = w - sx1;
            makePutImagesRecursive(msgs, contextId, layer, mode, x, y, image, bounds.adjusted(0, 0, -sx1, 0), estimatedSliceSize);
            makePutImagesRecursive(msgs, contextId, layer, mode, x + sx2, y, image, bounds.adjusted(sx2, 0, 0, 0), estimatedSliceSize);
        } else {
            int sy1 = h / 2;
            int sy2 = h - sy1;
            makePutImagesRecursive(msgs, contextId, layer, mode, x, y, image, bounds.adjusted(0, 0, 0, -sy1), estimatedSliceSize);
            makePutImagesRecursive(msgs, contextId, layer, mode, x, y + sy2, image, bounds.adjusted(0, sy2, 0, 0), estimatedSliceSize);
        }
    }
}

unsigned char *Message::getDeserializeBuffer(void *user, size_t size)
{
    QByteArray *buffer = static_cast<QByteArray *>(user);
    buffer->resize(compat::castSize(size));
    return reinterpret_cast<unsigned char *>(buffer->data());
}

void Message::setUchars(size_t size, unsigned char *out, void *user)
{
    memcpy(out, user, size);
}

void Message::setUint8s(int count, uint8_t *out, void *user)
{
    memcpy(out, user, sizeof(uint8_t) * count);
}

void Message::setUint16s(int count, uint16_t *out, void *user)
{
    memcpy(out, user, sizeof(uint16_t) * count);
}

}
