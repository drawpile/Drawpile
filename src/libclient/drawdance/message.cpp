// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/local_state.h>
#include <dpmsg/msg_internal.h>
}

#include "libclient/drawdance/message.h"

#include "libclient/canvas/blendmodes.h"
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/tile.h"
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


Message Message::makeAnnotationCreate(unsigned int contextId, uint16_t id, int32_t x, int32_t y, uint16_t w, uint16_t h)
{
    return Message{DP_msg_annotation_create_new(contextId, id, x, y, w, h)};
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

Message Message::makeAnnotationReshape(uint8_t contextId, uint16_t id, int32_t x, int32_t y, uint16_t w, uint16_t h)
{
    return Message{DP_msg_annotation_reshape_new(contextId, id, x, y, w, h)};
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

Message Message::makeDisconnect(uint8_t contextId, uint8_t reason, const QString &message)
{
    QByteArray bytes = message.toUtf8();
    return Message{DP_msg_disconnect_new(contextId, reason, bytes.constData(), bytes.length())};
}

Message Message::makeFeatureAccessLevels(uint8_t contextId, int featureCount, const uint8_t *features)
{
    return Message{DP_msg_feature_access_levels_new(contextId, setUint8s, featureCount, const_cast<uint8_t *>(features))};
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

Message Message::makeKeyFrameLayerAttributes(uint8_t contextId, uint16_t trackId, uint16_t frameIndex, const QVector<uint16_t> &layers)
{
    return Message{DP_msg_key_frame_layer_attributes_new(contextId, trackId, frameIndex, setUint16s, layers.size(), const_cast<uint16_t *>(layers.constData()))};
}

Message Message::makeKeyFrameSet(uint8_t contextId, uint16_t trackId, uint16_t frameIndex, uint16_t sourceId, uint16_t sourceIndex, uint8_t source)
{
    return Message{DP_msg_key_frame_set_new(contextId, trackId, frameIndex, sourceId, sourceIndex, source)};
}

Message Message::makeKeyFrameRetitle(uint8_t contextId, uint16_t trackId, uint16_t frameIndex, const QString &title)
{
    QByteArray bytes = title.toUtf8();
    return Message{DP_msg_key_frame_retitle_new(contextId, trackId, frameIndex, bytes.constData(), bytes.length())};
}

Message Message::makeKeyFrameDelete(uint8_t contextId, uint16_t trackId, uint16_t frameIndex, uint16_t moveTrackId, uint16_t moveFrameIndex)
{
    return Message{DP_msg_key_frame_delete_new(contextId, trackId, frameIndex, moveTrackId, moveFrameIndex)};
}

Message Message::makeLaserTrail(uint8_t contextId, uint32_t color, uint8_t persistence)
{
    return Message{DP_msg_laser_trail_new(contextId, color, persistence)};
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

Message Message::makeLayerCreate(uint8_t contextId, uint16_t id, uint16_t source, uint32_t fill, uint8_t flags, const QString &name)
{
    QByteArray bytes = name.toUtf8();
    return Message{DP_msg_layer_create_new(
        contextId, id, source, fill, flags, bytes.constData(), bytes.length())};
}

Message Message::makeLayerTreeCreate(uint8_t contextId, uint16_t id, uint16_t source, uint16_t target, uint32_t fill, uint8_t flags, const QString &name)
{
    QByteArray bytes = name.toUtf8();
    return Message{DP_msg_layer_tree_create_new(
        contextId, id, source, target, fill, flags, bytes.constData(), bytes.length())};
}

Message Message::makeLayerDelete(uint8_t contextId, uint16_t id, bool merge)
{
    return Message{DP_msg_layer_delete_new(contextId, id, merge)};
}

Message Message::makeLayerTreeDelete(uint8_t contextId, uint16_t id, uint16_t mergeTo)
{
    return Message{DP_msg_layer_tree_delete_new(contextId, id, mergeTo)};
}

Message Message::makeLayerRetitle(uint8_t contextId, uint16_t id, const QString &title)
{
    QByteArray bytes = title.toUtf8();
    return Message{DP_msg_layer_retitle_new(contextId, id, bytes.constData(), bytes.length())};
}

Message Message::makeMovePointer(uint8_t contextId, int32_t x, int32_t y)
{
    return Message{DP_msg_move_pointer_new(contextId, x, y)};
}

Message Message::makeMoveRegion(uint8_t contextId, uint16_t layer, int32_t bx, int32_t by, int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4, const QImage &mask)
{
    QByteArray compressed = mask.isNull() ? QByteArray{} : qCompress(mask.constBits(), mask.sizeInBytes());
    if(compressed.size() <= DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_MOVE_REGION_STATIC_LENGTH) {
        return Message{DP_msg_move_region_new(contextId, layer, bx, by, bw, bh, x1, y1, x2, y2, x3, y3, x4, y4, &Message::setUchars, compressed.size(), compressed.data())};
    } else {
        return Message::null();
    }
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

Message Message::makeTransformRegion(uint8_t contextId, uint16_t layer, uint16_t source, int32_t bx, int32_t by, int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode, const QImage &mask)
{
    QByteArray compressed = mask.isNull() ? QByteArray{} : compressAlphaMask(mask);
    if(compressed.size() <= DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_TRANSFORM_REGION_STATIC_LENGTH) {
        return Message{DP_msg_transform_region_new(contextId, layer, source, bx, by, bw, bh, x1, y1, x2, y2, x3, y3, x4, y4, mode, &Message::setUchars, compressed.size(), compressed.data())};
    } else {
        return Message::null();
    }
}

Message Message::makePing(uint8_t contextId, bool isPong)
{
    return Message{DP_msg_ping_new(contextId, isPong)};
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

Message Message::makeServerCommand(uint8_t contextId, const QJsonDocument &msg)
{
    QByteArray msgBytes = msg.toJson(QJsonDocument::Compact);
	if(msgBytes.length() <= DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_SERVER_COMMAND_STATIC_LENGTH) {
        return Message{DP_msg_server_command_new(contextId, msgBytes.constData(), msgBytes.length())};
    } else {
		qWarning("ServerCommand too long (%lld bytes)", compat::cast<long long>(msgBytes.length()));
		return drawdance::Message::null();
	}
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

Message Message::makeTrackCreate(uint8_t contextId, uint16_t id, uint16_t insertId, uint16_t sourceId, const QString &title)
{
    QByteArray bytes = title.toUtf8();
    return Message{DP_msg_track_create_new(contextId, id, insertId, sourceId, bytes.constData(), bytes.length())};
}

Message Message::makeTrackDelete(uint8_t contextId, uint16_t id)
{
    return Message{DP_msg_track_delete_new(contextId, id)};
}

Message Message::makeTrackOrder(uint8_t contextId, const QVector<uint16_t> &tracks)
{
    return Message{DP_msg_track_order_new(contextId, setUint16s, tracks.size(), const_cast<uint16_t *>(tracks.constData()))};
}

Message Message::makeTrackRetitle(uint8_t contextId, uint16_t id, const QString &title)
{
    QByteArray bytes = title.toUtf8();
    return Message{DP_msg_track_retitle_new(contextId, id, bytes.constData(), bytes.length())};
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


Message Message::makeLocalChangeLayerVisibility(int layerId, bool hidden)
{
    return Message{DP_local_state_msg_layer_visibility_new(layerId, hidden)};
}

Message Message::makeLocalChangeBackgroundColor(const QColor &color)
{
    DrawContext drawContext = DrawContextPool::acquire();
    Tile tile = Tile::fromColor(color);
    return Message{
        DP_local_state_msg_background_tile_new(drawContext.get(), tile.get())};
}

Message Message::makeLocalChangeBackgroundClear()
{
    return Message{DP_local_state_msg_background_tile_new(nullptr, nullptr)};
}


Message Message::makeLocalChangeViewMode(DP_ViewMode viewMode)
{
    return Message{DP_local_state_msg_view_mode_new(viewMode)};
}

Message Message::makeLocalChangeActiveLayer(int layerId)
{
    return Message{DP_local_state_msg_active_layer_new(layerId)};
}

Message Message::makeLocalChangeActiveFrame(int frameIndex)
{
    return Message{DP_local_state_msg_active_frame_new(frameIndex)};
}

Message Message::makeLocalChangeOnionSkins(const DP_OnionSkins *oss)
{
    return Message{DP_local_state_msg_onion_skins_new(oss)};
}

Message Message::makeLocalChangeTrackVisibility(int trackId, bool hidden)
{
    return Message{DP_local_state_msg_track_visibility_new(trackId, hidden)};
}
Message Message::makeLocalChangeTrackOnionSkin(int trackId, bool onionSkin)
{
    return Message{DP_local_state_msg_track_onion_skin_new(trackId, onionSkin)};
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

QString Message::typeName() const
{
    return QString::fromUtf8(DP_message_type_name(type()));
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

Message Message::makeBackwardCompatible() const
{
    switch(type()) {
    case DP_MSG_SERVER_COMMAND:
    case DP_MSG_DISCONNECT:
    case DP_MSG_PING:
    case DP_MSG_INTERNAL:
    case DP_MSG_JOIN:
    case DP_MSG_LEAVE:
    case DP_MSG_SESSION_OWNER:
    case DP_MSG_CHAT:
    case DP_MSG_TRUSTED_USERS:
    case DP_MSG_SOFT_RESET:
    case DP_MSG_PRIVATE_CHAT:
    case DP_MSG_INTERVAL:
    case DP_MSG_LASER_TRAIL:
    case DP_MSG_MOVE_POINTER:
    case DP_MSG_MARKER:
    case DP_MSG_LAYER_ACL:
    case DP_MSG_DEFAULT_LAYER:
    case DP_MSG_FILTERED:
    case DP_MSG_EXTENSION:
    case DP_MSG_UNDO_POINT:
    case DP_MSG_CANVAS_RESIZE:
    case DP_MSG_LAYER_CREATE:
    case DP_MSG_LAYER_RETITLE:
    case DP_MSG_LAYER_ORDER:
    case DP_MSG_LAYER_DELETE:
    case DP_MSG_PEN_UP:
    case DP_MSG_ANNOTATION_CREATE:
    case DP_MSG_ANNOTATION_RESHAPE:
    case DP_MSG_ANNOTATION_EDIT:
    case DP_MSG_ANNOTATION_DELETE:
    case DP_MSG_MOVE_REGION:
    case DP_MSG_PUT_TILE:
    case DP_MSG_CANVAS_BACKGROUND:
    case DP_MSG_UNDO:
        return *this;
    case DP_MSG_USER_ACL: {
        DP_MsgUserAcl *mua = DP_msg_user_acl_cast(m_data);
        int count;
        const uint8_t *users = DP_msg_user_acl_users(mua, &count);
        QVector<uint8_t> compatibleUsers;
        compatibleUsers.reserve(count);
        for(int i = 0; i < count; ++i) {
            uint8_t user = users[i];
            if(user != 0) { // Reset locks aren't in Drawpile 2.1.
                compatibleUsers.append(user);
            }
        }
        int compatibleCount = compatibleUsers.size();
        if(compatibleCount == count) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            return noinc(DP_msg_feature_access_levels_new(
                contextId(), setUint8s, compatibleCount, compatibleUsers.data()));
        }
    }
    case DP_MSG_FEATURE_ACCESS_LEVELS: {
        DP_MsgFeatureAccessLevels *mfal = DP_msg_feature_access_levels_cast(m_data);
        int count;
        const uint8_t *tiers =
            DP_msg_feature_access_levels_feature_tiers(mfal, &count);
        if(count == 9) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            uint8_t compatibleTiers[9];
            for(int i = 0; i < 9; ++i) {
                compatibleTiers[i] = i < count ? tiers[i] : 0;
            }
            return noinc(DP_msg_feature_access_levels_new(
                contextId(), setUint8s, 9, compatibleTiers));
        }
    }
    case DP_MSG_LAYER_ATTRIBUTES: {
        DP_MsgLayerAttributes *mla = DP_msg_layer_attributes_cast(m_data);
        bool compatible = canvas::blendmode::isBackwardCompatibleMode(
            DP_BlendMode(DP_msg_layer_attributes_blend(mla)));
        if(compatible) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            return noinc(DP_msg_layer_attributes_new(
                contextId(), DP_msg_layer_attributes_id(mla),
                DP_msg_layer_attributes_sublayer(mla),
                DP_msg_layer_attributes_flags(mla),
                DP_msg_layer_attributes_opacity(mla), DP_BLEND_MODE_NORMAL));
        }
    }
    case DP_MSG_PUT_IMAGE: {
        DP_MsgPutImage *mpi = DP_msg_put_image_cast(m_data);
        bool compatible = canvas::blendmode::isBackwardCompatibleMode(
            DP_BlendMode(DP_msg_put_image_mode(mpi)));
        if(compatible) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            size_t size;
            const unsigned char *image = DP_msg_put_image_image(mpi, &size);
            return noinc(DP_msg_put_image_new(
                contextId(), DP_msg_put_image_layer(mpi),
                DP_BLEND_MODE_NORMAL, DP_msg_put_image_x(mpi),
                DP_msg_put_image_y(mpi), DP_msg_put_image_w(mpi),
                DP_msg_put_image_h(mpi), setUchars, size,
                const_cast<unsigned char *>(image)));
        }
    }
    case DP_MSG_FILL_RECT: {
        DP_MsgFillRect *mfr = DP_msg_fill_rect_cast(m_data);
        bool compatible = canvas::blendmode::isBackwardCompatibleMode(
            DP_BlendMode(DP_msg_fill_rect_mode(mfr)));
        if(compatible) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            return noinc(DP_msg_fill_rect_new(
                contextId(), DP_msg_fill_rect_layer(mfr), DP_BLEND_MODE_NORMAL,
                DP_msg_fill_rect_x(mfr), DP_msg_fill_rect_y(mfr),
                DP_msg_fill_rect_w(mfr), DP_msg_fill_rect_h(mfr),
                DP_msg_fill_rect_color(mfr)));
        }
    }
    case DP_MSG_DRAW_DABS_CLASSIC: {
        DP_MsgDrawDabsClassic *mddc = DP_msg_draw_dabs_classic_cast(m_data);
        bool compatible = canvas::blendmode::isBackwardCompatibleMode(
            DP_BlendMode(DP_msg_draw_dabs_classic_mode(mddc)));
        if(compatible) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            int count;
            const DP_ClassicDab *cds =
                DP_msg_draw_dabs_classic_dabs(mddc, &count);
            return noinc(DP_msg_draw_dabs_classic_new(
                contextId(), DP_msg_draw_dabs_classic_layer(mddc),
                DP_msg_draw_dabs_classic_x(mddc),
                DP_msg_draw_dabs_classic_y(mddc),
                DP_msg_draw_dabs_classic_color(mddc), DP_BLEND_MODE_NORMAL,
                setClassicDabs, count, const_cast<DP_ClassicDab *>(cds)));
        }
    }
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE: {
        DP_MsgDrawDabsPixel *mddp =
            static_cast<DP_MsgDrawDabsPixel *>(DP_message_internal(m_data));
        bool compatible = canvas::blendmode::isBackwardCompatibleMode(
            DP_BlendMode(DP_msg_draw_dabs_pixel_mode(mddp)));
        if(compatible) {
            return *this;
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            int count;
            const DP_PixelDab *pds = DP_msg_draw_dabs_pixel_dabs(mddp, &count);
            return noinc((type() == DP_MSG_DRAW_DABS_PIXEL
                    ? DP_msg_draw_dabs_pixel_new
                    : DP_msg_draw_dabs_pixel_square_new)(
                contextId(), DP_msg_draw_dabs_pixel_layer(mddp),
                DP_msg_draw_dabs_pixel_x(mddp), DP_msg_draw_dabs_pixel_y(mddp),
                DP_msg_draw_dabs_pixel_color(mddp), DP_BLEND_MODE_NORMAL,
                setPixelDabs, count, const_cast<DP_PixelDab *>(pds)));
        }
    }
    case DP_MSG_LAYER_TREE_CREATE: {
        DP_MsgLayerTreeCreate *mltc = DP_msg_layer_tree_create_cast(m_data);
        uint8_t flags = DP_msg_layer_tree_create_flags(mltc);
        uint16_t sourceId = DP_msg_layer_tree_create_source(mltc);
        uint16_t targetId = DP_msg_layer_tree_create_target(mltc);
        bool involvesGroups = (flags & DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP)
            || (flags & DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO);
        bool sourceAndTargetDiffer =
            sourceId != 0 && targetId != 0 && sourceId != targetId;
        if(involvesGroups || sourceAndTargetDiffer) {
            return null();
        } else {
            qDebug("Making %s message compatible", qUtf8Printable(typeName()));
            uint8_t compatFlags =
                (sourceId == 0 ? 0 : DP_MSG_LAYER_CREATE_FLAGS_COPY) |
                (targetId == 0 ? 0 : DP_MSG_LAYER_CREATE_FLAGS_INSERT);
            uint16_t compatSourceId = sourceId == 0 ? targetId : sourceId;
            size_t titleLength;
            const char *title =
                DP_msg_layer_tree_create_title(mltc, &titleLength);
            return noinc(DP_msg_layer_create_new(
                contextId(), DP_msg_layer_tree_create_id(mltc), compatSourceId,
                DP_msg_layer_tree_create_fill(mltc), compatFlags, title,
                titleLength));
        }
    }
    default:
        return null();
    }
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
    if(size > 0) {
        memcpy(out, user, size);
    }
}

void Message::setUint8s(int count, uint8_t *out, void *user)
{
    if(count > 0) {
        memcpy(out, user, sizeof(uint8_t) * count);
    }
}

void Message::setUint16s(int count, uint16_t *out, void *user)
{
    if(count > 0) {
        memcpy(out, user, sizeof(uint16_t) * count);
    }
}

void Message::setClassicDabs(int count, DP_ClassicDab *out, void *user)
{
    const DP_ClassicDab *cds = static_cast<const DP_ClassicDab *>(user);
    for(int i = 0; i < count; ++i) {
        const DP_ClassicDab *cd = DP_classic_dab_at(cds, i);
        DP_classic_dab_init(
            out, i, DP_classic_dab_x(cd), DP_classic_dab_y(cd),
            DP_classic_dab_size(cd), DP_classic_dab_hardness(cd),
            DP_classic_dab_opacity(cd));
    }
}

void Message::setPixelDabs(int count, DP_PixelDab *out, void *user)
{
    const DP_PixelDab *pds = static_cast<const DP_PixelDab *>(user);
    for(int i = 0; i < count; ++i) {
        const DP_PixelDab *pd = DP_pixel_dab_at(pds, i);
        DP_pixel_dab_init(
            out, i, DP_pixel_dab_x(pd), DP_pixel_dab_y(pd),
            DP_pixel_dab_size(pd), DP_pixel_dab_opacity(pd));
    }
}

}
