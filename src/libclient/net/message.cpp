// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/compress.h>
#include <dpengine/local_state.h>
#include <dpmsg/ids.h>
#include <dpmsg/msg_internal.h>
}
#include "libclient/canvas/blendmodes.h"
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/tile.h"
#include "libclient/net/message.h"
#include "libshared/util/qtcompat.h"
#include <QByteArray>
#include <QImage>
#include <QJsonDocument>
#include <QString>
#include <QtEndian>

namespace net {

Message makeAnnotationCreateMessage(
	unsigned int contextId, uint16_t id, int32_t x, int32_t y, uint16_t w,
	uint16_t h)
{
	return Message::noinc(
		DP_msg_annotation_create_new(contextId, id, x, y, w, h));
}

Message makeAnnotationDeleteMessage(uint8_t contextId, uint16_t id)
{
	return Message::noinc(DP_msg_annotation_delete_new(contextId, id));
}

Message makeAnnotationEditMessage(
	uint8_t contextId, uint16_t id, uint32_t bg, uint8_t flags, uint8_t border,
	const QString &text)
{
	QByteArray bytes = text.toUtf8();
	return Message::noinc(DP_msg_annotation_edit_new(
		contextId, id, bg, flags, border, bytes.constData(), bytes.length()));
}

Message makeAnnotationReshapeMessage(
	uint8_t contextId, uint16_t id, int32_t x, int32_t y, uint16_t w,
	uint16_t h)
{
	return Message::noinc(
		DP_msg_annotation_reshape_new(contextId, id, x, y, w, h));
}

Message makeCameraCreateMessage(
	uint8_t contextId, uint16_t id, uint16_t sourceId, const QString &title)
{
	QByteArray bytes = title.toUtf8();
	return Message::noinc(DP_msg_camera_create_new(
		contextId, id, sourceId, bytes.constData(), bytes.length()));
}

Message makeCameraDeleteMessage(uint8_t contextId, uint16_t id)
{
	return Message::noinc(DP_msg_camera_delete_new(contextId, id));
}

Message makeCanvasBackgroundMessage(uint8_t contextId, const QColor &color)
{
	uint32_t c = qToBigEndian(color.rgba());
	return Message::noinc(DP_msg_canvas_background_new(
		contextId, Message::setUchars, sizeof(c), &c));
}

Message makeCanvasResizeMessage(
	uint8_t contextId, int32_t top, int32_t right, int32_t bottom, int32_t left)
{
	return Message::noinc(
		DP_msg_canvas_resize_new(contextId, top, right, bottom, left));
}

Message makeDefaultLayerMessage(uint8_t contextId, uint32_t id)
{
	return Message::noinc(DP_msg_default_layer_new(contextId, id));
}

Message makeFeatureAccessLevelsMessage(
	uint8_t contextId, const QVector<uint8_t> &features)
{
	return Message::noinc(DP_msg_feature_access_levels_new(
		contextId, Message::setUint8s, features.size(),
		const_cast<uint8_t *>(features.constData())));
}

Message
makeFeatureLimitsMessage(uint8_t contextId, const QVector<int32_t> &limits)
{
	return Message::noinc(DP_msg_feature_limits_new(
		contextId, Message::setInt32s, limits.size(),
		const_cast<int32_t *>(limits.constData())));
}

Message makeFillRectMessage(
	uint8_t contextId, uint32_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QColor &color)
{
	return Message::noinc(
		DP_msg_fill_rect_new(contextId, layer, mode, x, y, w, h, color.rgba()));
}

Message makeInternalCatchupMessage(uint8_t contextId, int progress)
{
	return Message::noinc(DP_msg_internal_catchup_new(contextId, progress));
}

Message makeInternalCleanupMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_internal_cleanup_new(contextId));
}

Message makeInternalPaintSyncMessage(
	uint8_t contextId, void (*callback)(void *), void *user)
{
	return Message::noinc(
		DP_msg_internal_paint_sync_new(contextId, callback, user));
}

Message makeInternalReconnectStateApplyMessage(
	uint8_t contextId, DP_CanvasHistoryReconnectState *chrs)
{
	return Message::noinc(
		DP_msg_internal_reconnect_state_apply_new(contextId, chrs));
}

Message makeInternalReconnectStateMakeMessage(
	uint8_t contextId,
	void (*callback)(void *, DP_CanvasHistoryReconnectState *), void *user)
{
	return Message::noinc(
		DP_msg_internal_reconnect_state_make_new(contextId, callback, user));
}

Message makeInternalResetMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_internal_reset_new(contextId));
}

Message makeInternalStreamResetStartMessage(
	uint8_t contextId, const QString &correlator)
{
	QByteArray bytes = correlator.toUtf8();
	return Message::noinc(DP_msg_internal_stream_reset_start_new(
		contextId, bytes.size(), bytes.constData()));
}

Message makeInternalSnapshotMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_internal_snapshot_new(contextId));
}

Message makeKeyFrameLayerAttributesMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	const QVector<uint32_t> &layers)
{
	return Message::noinc(DP_msg_key_frame_layer_attributes_new(
		contextId, trackId, frameIndex, Message::setUint24s, layers.size(),
		const_cast<uint32_t *>(layers.constData())));
}

Message makeKeyFrameSetMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex, uint32_t sourceId,
	uint16_t sourceIndex, uint8_t source)
{
	return Message::noinc(DP_msg_key_frame_set_new(
		contextId, trackId, frameIndex, sourceId, sourceIndex, source));
}

Message makeKeyFrameRetitleMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	const QString &title)
{
	QByteArray bytes = title.toUtf8();
	return Message::noinc(DP_msg_key_frame_retitle_new(
		contextId, trackId, frameIndex, bytes.constData(), bytes.length()));
}

Message makeKeyFrameDeleteMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	uint16_t moveTrackId, uint16_t moveFrameIndex)
{
	return Message::noinc(DP_msg_key_frame_delete_new(
		contextId, trackId, frameIndex, moveTrackId, moveFrameIndex));
}

Message
makeLaserTrailMessage(uint8_t contextId, uint32_t color, uint8_t persistence)
{
	return Message::noinc(
		DP_msg_laser_trail_new(contextId, color, persistence));
}

Message makeLayerAttributesMessage(
	uint8_t contextId, uint32_t id, uint8_t sublayer, uint8_t flags,
	uint8_t opacity, uint8_t blend)
{
	return Message::noinc(DP_msg_layer_attributes_new(
		contextId, id, sublayer, flags, opacity, blend));
}

Message makeLayerAclMessage(
	uint8_t contextId, uint32_t id, uint8_t flags,
	const QVector<uint8_t> &exclusive)
{
	return Message::noinc(DP_msg_layer_acl_new(
		contextId, id, flags, Message::setUint8s, exclusive.count(),
		const_cast<uint8_t *>(exclusive.constData())));
}

Message makeLayerTreeCreateMessage(
	uint8_t contextId, uint32_t id, uint32_t source, uint32_t target,
	uint32_t fill, uint8_t flags, const QString &name)
{
	QByteArray bytes = name.toUtf8();
	return Message::noinc(DP_msg_layer_tree_create_new(
		contextId, id, source, target, fill, flags, bytes.constData(),
		bytes.length()));
}

Message
makeLayerTreeDeleteMessage(uint8_t contextId, uint32_t id, uint32_t mergeTo)
{
	return Message::noinc(DP_msg_layer_tree_delete_new(contextId, id, mergeTo));
}

Message makeLayerTreeMoveMessage(
	uint8_t contextId, uint32_t layer, uint32_t parent, uint32_t sibling)
{
	return Message::noinc(
		DP_msg_layer_tree_move_new(contextId, layer, parent, sibling));
}

Message
makeLayerRetitleMessage(uint8_t contextId, uint32_t id, const QString &title)
{
	QByteArray bytes = title.toUtf8();
	return Message::noinc(DP_msg_layer_retitle_new(
		contextId, id, bytes.constData(), bytes.length()));
}

Message makeMovePointerMessage(uint8_t contextId, int32_t x, int32_t y)
{
	return Message::noinc(DP_msg_move_pointer_new(contextId, x, y));
}

static unsigned char *getByteArrayBuffer(size_t size, void *user)
{
	QByteArray *buffer = static_cast<QByteArray *>(user);
	buffer->resize(compat::castSize(size));
	return reinterpret_cast<unsigned char *>(buffer->data());
}

static QByteArray
compressImageDeflate(QByteArray &compressionBuffer, const QImage &img)
{
	Q_ASSERT(img.format() == QImage::Format_ARGB32_Premultiplied);
	size_t size = DP_compress_deflate(
		reinterpret_cast<const unsigned char *>(img.constBits()),
		size_t(img.sizeInBytes()), getByteArrayBuffer, &compressionBuffer);
	compressionBuffer.truncate(compat::castSize(size));
	return compressionBuffer;
}

static QByteArray compressImageSplitDeltaZstd(
	ZSTD_CCtx **inOutCtx, QByteArray &splitBuffer,
	QByteArray &compressionBuffer, const QImage &img)
{
	Q_ASSERT(img.format() == QImage::Format_ARGB32_Premultiplied);
	int width = img.width();
	int height = img.height();
	int count = width * height;
	splitBuffer.resize(count * 4);
	DP_pixels8_to_split8_delta(
		reinterpret_cast<uint8_t *>(splitBuffer.data()),
		reinterpret_cast<const DP_Pixel8 *>(img.constBits()), count);

	size_t size = DP_compress_zstd(
		inOutCtx,
		reinterpret_cast<const unsigned char *>(splitBuffer.constData()),
		size_t(splitBuffer.size()), getByteArrayBuffer, &compressionBuffer);
	compressionBuffer.truncate(compat::castSize(size));
	return compressionBuffer;
}

static QByteArray compressAlphaMaskDeflate(
	QByteArray &alphaMaskBuffer, QByteArray &compressionBuffer,
	const QImage &mask)
{
	Q_ASSERT(mask.format() == QImage::Format_ARGB32_Premultiplied);
	int width = mask.width();
	int height = mask.height();
	alphaMaskBuffer.clear();
	alphaMaskBuffer.reserve(width * height);
	for(int y = 0; y < height; ++y) {
		const uchar *scanLine = mask.scanLine(y);
		for(int x = 0; x < width; ++x) {
			alphaMaskBuffer.append(scanLine[x * 4 + 3]);
		}
	}

	size_t size = DP_compress_deflate(
		reinterpret_cast<const unsigned char *>(alphaMaskBuffer.constData()),
		size_t(alphaMaskBuffer.size()), getByteArrayBuffer, &compressionBuffer);
	compressionBuffer.truncate(compat::castSize(size));
	return compressionBuffer;
}

static QByteArray compressAlphaMaskDeltaZstd(
	ZSTD_CCtx **inOutCtx, QByteArray &alphaMaskBuffer,
	QByteArray &compressionBuffer, const QImage &mask)
{
	Q_ASSERT(mask.format() == QImage::Format_ARGB32_Premultiplied);
	int width = mask.width();
	int height = mask.height();
	alphaMaskBuffer.clear();
	alphaMaskBuffer.reserve(width * height);
	uchar lastA = 0;
	for(int y = 0; y < height; ++y) {
		const uchar *scanLine = mask.scanLine(y);
		for(int x = 0; x < width; ++x) {
			uchar a = scanLine[x * 4 + 3];
			alphaMaskBuffer.append(a - lastA);
			lastA = a;
		}
	}

	size_t size = DP_compress_zstd(
		inOutCtx,
		reinterpret_cast<const unsigned char *>(alphaMaskBuffer.constData()),
		size_t(alphaMaskBuffer.size()), getByteArrayBuffer, &compressionBuffer);
	compressionBuffer.truncate(compat::castSize(size));
	return compressionBuffer;
}

static Message makeMoveRectDeflateMessage(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t sx, int32_t sy,
	int32_t tx, int32_t ty, int32_t w, int32_t h, uint8_t blend,
	uint8_t opacity, const QImage &mask)
{
	QByteArray compressed;
	if(!mask.isNull()) {
		QByteArray alphaMaskBuffer;
		QByteArray compressionBuffer;
		compressed =
			compressAlphaMaskDeflate(alphaMaskBuffer, compressionBuffer, mask);
	}

	if(compressed.size() <=
	   DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_MOVE_RECT_STATIC_LENGTH) {
		return Message::noinc(DP_msg_move_rect_new(
			contextId, layer, source, sx, sy, tx, ty, w, h, blend, opacity,
			&Message::setUchars, compressed.size(), compressed.data()));
	} else {
		return Message::null();
	}
}

static Message makeMoveRectZstdMessage(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t sx, int32_t sy,
	int32_t tx, int32_t ty, int32_t w, int32_t h, uint8_t blend,
	uint8_t opacity, const QImage &mask)
{
	QByteArray compressed;
	if(!mask.isNull()) {
		QByteArray alphaMaskBuffer;
		QByteArray compressionBuffer;
		compressed = compressAlphaMaskDeltaZstd(
			nullptr, alphaMaskBuffer, compressionBuffer, mask);
	}

	if(compressed.size() <=
	   DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_MOVE_RECT_STATIC_LENGTH) {
		return Message::noinc(DP_msg_move_rect_zstd_new(
			contextId, layer, source, sx, sy, tx, ty, w, h, blend, opacity,
			&Message::setUchars, compressed.size(), compressed.data()));
	} else {
		return Message::null();
	}
}

Message makeMoveRectMessageCompat(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t sx, int32_t sy,
	int32_t tx, int32_t ty, int32_t w, int32_t h, uint8_t blend,
	uint8_t opacity, const QImage &mask, bool compatibilityMode)
{
	if(compatibilityMode && !(layer & DP_LAYER_ID_SELECTION_FLAG)) {
		return makeMoveRectDeflateMessage(
			contextId, layer, source, sx, sy, tx, ty, w, h,
			uint8_t(DP_BLEND_MODE_NORMAL), 255, mask);
	} else {
		return makeMoveRectZstdMessage(
			contextId, layer, source, sx, sy, tx, ty, w, h, blend, opacity,
			mask);
	}
}

static Message makeTransformRegionDeflateMessage(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t bx, int32_t by,
	int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode, uint8_t blend,
	uint8_t opacity, const QImage &mask)
{
	QByteArray compressed;
	if(!mask.isNull()) {
		QByteArray alphaMaskBuffer;
		QByteArray compressionBuffer;
		compressed =
			compressAlphaMaskDeflate(alphaMaskBuffer, compressionBuffer, mask);
	}

	if(compressed.size() <=
	   DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_TRANSFORM_REGION_STATIC_LENGTH) {
		return Message::noinc(DP_msg_transform_region_new(
			contextId, layer, source, bx, by, bw, bh, x1, y1, x2, y2, x3, y3,
			x4, y4, mode, blend, opacity, &Message::setUchars,
			compressed.size(), compressed.data()));
	} else {
		return Message::null();
	}
}

static Message makeTransformRegionZstdMessage(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t bx, int32_t by,
	int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode, uint8_t blend,
	uint8_t opacity, const QImage &mask)
{
	QByteArray compressed;
	if(!mask.isNull()) {
		QByteArray alphaMaskBuffer;
		QByteArray compressionBuffer;
		compressed = compressAlphaMaskDeltaZstd(
			nullptr, alphaMaskBuffer, compressionBuffer, mask);
	}

	if(compressed.size() <=
	   DP_MESSAGE_MAX_PAYLOAD_LENGTH - DP_MSG_TRANSFORM_REGION_STATIC_LENGTH) {
		return Message::noinc(DP_msg_transform_region_zstd_new(
			contextId, layer, source, bx, by, bw, bh, x1, y1, x2, y2, x3, y3,
			x4, y4, mode, blend, opacity, &Message::setUchars,
			compressed.size(), compressed.data()));
	} else {
		return Message::null();
	}
}

Message makeTransformRegionMessageCompat(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t bx, int32_t by,
	int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode, uint8_t blend,
	uint8_t opacity, const QImage &mask, bool compatibilityMode)
{
	if(compatibilityMode && !(layer & DP_LAYER_ID_SELECTION_FLAG)) {
		return makeTransformRegionDeflateMessage(
			contextId, layer, source, bx, by, bw, bh, x1, y1, x2, y2, x3, y3,
			x4, y4, mode, uint8_t(DP_BLEND_MODE_NORMAL), 255, mask);
	} else {
		return makeTransformRegionZstdMessage(
			contextId, layer, source, bx, by, bw, bh, x1, y1, x2, y2, x3, y3,
			x4, y4, mode, blend, opacity, mask);
	}
}

Message makePutImageMessage(
	uint8_t contextId, uint32_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedImage)
{
	return Message::noinc(DP_msg_put_image_new(
		contextId, layer, mode, x, y, w, h, Message::setUchars,
		compressedImage.size(), const_cast<char *>(compressedImage.data())));
}

Message makePutImageZstdMessage(
	uint8_t contextId, uint32_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedImage)
{
	return Message::noinc(DP_msg_put_image_zstd_new(
		contextId, layer, mode, x, y, w, h, Message::setUchars,
		compressedImage.size(), const_cast<char *>(compressedImage.data())));
}

Message makeSelectionClearMessage(uint8_t contextId, uint8_t selectionId)
{
	return Message::noinc(DP_msg_selection_clear_new(contextId, selectionId));
}

Message makeSelectionPutMessage(
	uint8_t contextId, uint8_t selectionId, uint8_t op, int32_t x, int32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedMask)
{
	return Message::noinc(DP_msg_selection_put_new(
		contextId, selectionId, op, x, y, w, h, &Message::setUchars,
		compressedMask.size(), const_cast<char *>(compressedMask.data())));
}

Message
makeSetMetadataIntMessage(uint8_t contextId, uint8_t field, int32_t value)
{
	return Message::noinc(DP_msg_set_metadata_int_new(contextId, field, value));
}

Message makeTrackCreateMessage(
	uint8_t contextId, uint16_t id, uint16_t insertId, uint16_t sourceId,
	const QString &title)
{
	QByteArray bytes = title.toUtf8();
	return Message::noinc(DP_msg_track_create_new(
		contextId, id, insertId, sourceId, bytes.constData(), bytes.length()));
}

Message makeTrackDeleteMessage(uint8_t contextId, uint16_t id)
{
	return Message::noinc(DP_msg_track_delete_new(contextId, id));
}

Message
makeTrackOrderMessage(uint8_t contextId, const QVector<uint16_t> &tracks)
{
	return Message::noinc(DP_msg_track_order_new(
		contextId, Message::setUint16s, tracks.size(),
		const_cast<uint16_t *>(tracks.constData())));
}

Message
makeTrackRetitleMessage(uint8_t contextId, uint16_t id, const QString &title)
{
	QByteArray bytes = title.toUtf8();
	return Message::noinc(DP_msg_track_retitle_new(
		contextId, id, bytes.constData(), bytes.length()));
}

Message makeUndoMessage(uint8_t contextId, uint8_t overrideUser, bool redo)
{
	return Message::noinc(DP_msg_undo_new(contextId, overrideUser, redo));
}

Message makeUndoDepthMessage(uint8_t contextId, uint8_t depth)
{
	return Message::noinc(DP_msg_undo_depth_new(contextId, depth));
}

Message makeUndoPointMessage(uint8_t contextId)
{
	return Message::noinc(DP_msg_undo_point_new(contextId));
}

Message makeUserAclMessage(uint8_t contextId, const QVector<uint8_t> &users)
{
	return Message::noinc(DP_msg_user_acl_new(
		contextId, Message::setUint8s, users.count(),
		const_cast<uint8_t *>(users.constData())));
}

Message makeUserInfoMessage(
	uint8_t contextId, uint8_t recipient, const QJsonDocument &msg)
{
	QByteArray msgBytes = msg.toJson(QJsonDocument::Compact);
	return Message::noinc(DP_msg_data_new(
		contextId, DP_MSG_DATA_TYPE_USER_INFO, recipient, Message::setUchars,
		msgBytes.length(), const_cast<char *>(msgBytes.constData())));
}

namespace {
class PutImageMaker {
public:
	PutImageMaker(
		net::MessageList &msgs, uint8_t contextId, uint32_t layer, uint8_t mode)
		: m_msgs(msgs)
		, m_contextId(contextId)
		, m_layer(layer)
		, m_mode(mode)
	{
	}

	virtual ~PutImageMaker() {}

	void make(int x, int y, const QImage &image)
	{
		// If the image is totally outside of the canvas, there's nothing to do.
		if(x >= -image.width() && y >= -image.height()) {
			QImage converted =
				image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
			if(x < 0 || y < 0) {
				// Crop image, since the protocol doesn't do negative
				// coordinates.
				int xoffset = x < 0 ? -x : 0;
				int yoffset = y < 0 ? -y : 0;
				QImage cropped = converted.copy(
					xoffset, yoffset, image.width() - xoffset,
					image.height() - yoffset);
				makeRecursive(
					x + xoffset, y + yoffset, cropped, cropped.rect(), 0);
			} else {
				makeRecursive(x, y, converted, converted.rect(), 0);
			}
		}
	}

protected:
	static constexpr compat::sizetype MAX_SIZE =
		DP_MSG_PUT_IMAGE_IMAGE_MAX_SIZE;

	virtual QByteArray compress(const QImage &image) = 0;

	virtual net::Message
	makeMessage(int x, int y, int w, int h, const QByteArray &compressed) = 0;

	void makeRecursive(
		int x, int y, const QImage &image, const QRect &bounds,
		int estimatedSize)
	{
		int w = bounds.width();
		int h = bounds.height();
		if(w > 0 && h > 0) {
			int compressedSize;
			// If our estimated size looks good, try compressing. Otherwise
			// assume that the image is too big to fit into a message and split
			// it up.
			if(estimatedSize < MAX_SIZE) {
				QImage subImage =
					bounds == image.rect() ? image : image.copy(bounds);
				QByteArray compressed = compress(subImage);
				compressedSize = compressed.size();
				if(compressedSize <= MAX_SIZE) {
					m_msgs.append(makeMessage(x, y, w, h, compressed));
					return;
				}
			} else {
				compressedSize = estimatedSize;
			}
			// (Probably) too big to fit in a message, slice in half along the
			// longest axis.
			int estimatedSliceSize = compressedSize / 2;
			if(w > h) {
				int sx1 = w / 2;
				int sx2 = w - sx1;
				makeRecursive(
					x, y, image, bounds.adjusted(0, 0, -sx1, 0),
					estimatedSliceSize);
				makeRecursive(
					x + sx2, y, image, bounds.adjusted(sx2, 0, 0, 0),
					estimatedSliceSize);
			} else {
				int sy1 = h / 2;
				int sy2 = h - sy1;
				makeRecursive(
					x, y, image, bounds.adjusted(0, 0, 0, -sy1),
					estimatedSliceSize);
				makeRecursive(
					x, y + sy2, image, bounds.adjusted(0, sy2, 0, 0),
					estimatedSliceSize);
			}
		}
	}

	MessageList &m_msgs;
	const uint8_t m_contextId;
	const uint32_t m_layer;
	const uint8_t m_mode;
};

class PutImageMakerDeflate final : public PutImageMaker {
public:
	PutImageMakerDeflate(
		net::MessageList &msgs, uint8_t contextId, uint32_t layer, uint8_t mode)
		: PutImageMaker(msgs, contextId, layer, mode)
	{
	}

protected:
	QByteArray compress(const QImage &image) override
	{
		return compressImageDeflate(m_compressionBuffer, image);
	}

	net::Message makeMessage(
		int x, int y, int w, int h, const QByteArray &compressed) override
	{
		return makePutImageMessage(
			m_contextId, m_layer, m_mode, x, y, w, h, compressed);
	}

private:
	QByteArray m_compressionBuffer;
};

class PutImageMakerZstd final : public PutImageMaker {
public:
	PutImageMakerZstd(
		net::MessageList &msgs, uint8_t contextId, uint32_t layer, uint8_t mode)
		: PutImageMaker(msgs, contextId, layer, mode)
	{
	}

	~PutImageMakerZstd() override { DP_compress_zstd_free(&m_ctx); }

protected:
	QByteArray compress(const QImage &image) override
	{
		return compressImageSplitDeltaZstd(
			&m_ctx, m_splitBuffer, m_compressionBuffer, image);
	}

	net::Message makeMessage(
		int x, int y, int w, int h, const QByteArray &compressed) override
	{
		return makePutImageZstdMessage(
			m_contextId, m_layer, m_mode, x, y, w, h, compressed);
	}

private:
	ZSTD_CCtx *m_ctx = nullptr;
	QByteArray m_splitBuffer;
	QByteArray m_compressionBuffer;
};
}

void makePutImageMessagesCompat(
	MessageList &msgs, uint8_t contextId, uint32_t layer, uint8_t mode, int x,
	int y, const QImage &image, bool compatibilityMode)
{
	if(compatibilityMode && !(layer & DP_LAYER_ID_SELECTION_FLAG)) {
		PutImageMakerDeflate(msgs, contextId, layer, mode).make(x, y, image);
	} else {
		PutImageMakerZstd(msgs, contextId, layer, mode).make(x, y, image);
	}
}

static void makeSelectionsPutRecursive(
	ZSTD_CCtx **inOutCtx, QByteArray &alphaMaskBuffer,
	QByteArray &compressionBuffer, MessageList &msgs, uint8_t contextId,
	uint8_t selectionId, uint8_t &op, int x, int y, const QImage &image,
	const QRect &bounds, int estimatedSize)
{
	int w = bounds.width();
	int h = bounds.height();
	if(w > 0 && h > 0) {
		int maxSize = DP_MSG_SELECTION_PUT_MASK_MAX_SIZE;
		int compressedSize;
		if(estimatedSize < maxSize) {
			QImage subImage =
				bounds == image.rect() ? image : image.copy(bounds);
			QByteArray compressed = compressAlphaMaskDeltaZstd(
				inOutCtx, alphaMaskBuffer, compressionBuffer, subImage);
			compressedSize = compressed.size();
			if(compressedSize <= maxSize) {
				msgs.append(makeSelectionPutMessage(
					contextId, selectionId, op, x, y, w, h, compressed));
				if(op == DP_MSG_SELECTION_PUT_OP_REPLACE) {
					op = DP_MSG_SELECTION_PUT_OP_UNITE;
				}
				return;
			}
		} else {
			compressedSize = estimatedSize;
		}

		int estimatedSliceSize = compressedSize / 2;
		if(w > h) {
			int sx1 = w / 2;
			int sx2 = w - sx1;
			makeSelectionsPutRecursive(
				inOutCtx, alphaMaskBuffer, compressionBuffer, msgs, contextId,
				selectionId, op, x, y, image, bounds.adjusted(0, 0, -sx1, 0),
				estimatedSliceSize);
			makeSelectionsPutRecursive(
				inOutCtx, alphaMaskBuffer, compressionBuffer, msgs, contextId,
				selectionId, op, x + sx2, y, image,
				bounds.adjusted(sx2, 0, 0, 0), estimatedSliceSize);
		} else {
			int sy1 = h / 2;
			int sy2 = h - sy1;
			makeSelectionsPutRecursive(
				inOutCtx, alphaMaskBuffer, compressionBuffer, msgs, contextId,
				selectionId, op, x, y, image, bounds.adjusted(0, 0, 0, -sy1),
				estimatedSliceSize);
			makeSelectionsPutRecursive(
				inOutCtx, alphaMaskBuffer, compressionBuffer, msgs, contextId,
				selectionId, op, x, y + sy2, image,
				bounds.adjusted(0, sy2, 0, 0), estimatedSliceSize);
		}
	}
}

void makeSelectionPutMessages(
	MessageList &msgs, uint8_t contextId, uint8_t selectionId, uint8_t op,
	int x, int y, int w, int h, const QImage &image)
{
	if(image.isNull()) {
		msgs.append(makeSelectionPutMessage(
			contextId, selectionId, op, x, y, w, h, QByteArray()));
	} else if(x >= -image.width() && y >= -image.height()) {
		QImage converted =
			image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
		ZSTD_CCtx *ctx = nullptr;
		if(x < 0 || y < 0) {
			int xoffset = x < 0 ? -x : 0;
			int yoffset = y < 0 ? -y : 0;
			QImage cropped = converted.copy(
				xoffset, yoffset, image.width() - xoffset,
				image.height() - yoffset);
			QByteArray alphaMaskBuffer;
			QByteArray compressionBuffer;
			makeSelectionsPutRecursive(
				&ctx, alphaMaskBuffer, compressionBuffer, msgs, contextId,
				selectionId, op, x + xoffset, y + yoffset, cropped,
				cropped.rect(), 0);
		} else {
			QByteArray alphaMaskBuffer;
			QByteArray compressionBuffer;
			makeSelectionsPutRecursive(
				&ctx, alphaMaskBuffer, compressionBuffer, msgs, contextId,
				selectionId, op, x, y, converted, converted.rect(), 0);
		}
		DP_compress_zstd_free(&ctx);
	}
}

Message makeLocalChangeLayerVisibilityMessage(int layerId, bool hidden)
{
	return Message::noinc(
		DP_local_state_msg_layer_visibility_new(layerId, hidden));
}

Message makeLocalChangeBackgroundColorMessage(const QColor &color)
{
	drawdance::DrawContext drawContext = drawdance::DrawContextPool::acquire();
	drawdance::Tile tile = drawdance::Tile::fromColor(color);
	return Message::noinc(
		DP_local_state_msg_background_tile_new(drawContext.get(), tile.get()));
}

Message makeLocalChangeBackgroundClearMessage()
{
	return Message::noinc(
		DP_local_state_msg_background_tile_new(nullptr, nullptr));
}


Message makeLocalChangeViewModeMessage(DP_ViewMode viewMode)
{
	return Message::noinc(DP_local_state_msg_view_mode_new(viewMode));
}

Message makeLocalChangeActiveLayerMessage(int layerId)
{
	return Message::noinc(DP_local_state_msg_active_layer_new(layerId));
}

Message makeLocalChangeActiveFrameMessage(int frameIndex)
{
	return Message::noinc(DP_local_state_msg_active_frame_new(frameIndex));
}

Message makeLocalChangeOnionSkinsMessage(const DP_OnionSkins *oss)
{
	return Message::noinc(DP_local_state_msg_onion_skins_new(oss));
}

Message makeLocalChangeTrackVisibilityMessage(int trackId, bool hidden)
{
	return Message::noinc(
		DP_local_state_msg_track_visibility_new(trackId, hidden));
}

Message makeLocalChangeTrackOnionSkinMessage(int trackId, bool onionSkin)
{
	return Message::noinc(
		DP_local_state_msg_track_onion_skin_new(trackId, onionSkin));
}

Message makeLocalChangeLayerSketchMessage(
	int layerId, uint16_t opacity, const QColor &tint)
{
	return Message::noinc(
		DP_local_state_msg_layer_sketch_new(layerId, opacity, tint.rgba()));
}

Message makeLocalChangeLayerAlphaLockMessage(int layerId, bool alphaLock)
{
	return Message::noinc(
		DP_local_state_msg_layer_alpha_lock_new(layerId, alphaLock));
}

Message makeLocalChangeLayerCensoredMessage(int layerId, bool censored)
{
	return Message::noinc(
		DP_local_state_msg_layer_censored_new(layerId, censored));
}

DP_Message *makeLocalMatchMessage(const Message &msg, bool disguiseAsPutImage)
{
	return DP_msg_local_match_make(msg.get(), disguiseAsPutImage);
}
}
