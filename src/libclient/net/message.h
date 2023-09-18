// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_MESSAGE_H
#define LIBCLIENT_DRAWDANCE_MESSAGE_H
extern "C" {
#include <dpengine/view_mode.h>
#include <dpmsg/message.h>
}
#include <QRect>
#include <libshared/net/message.h>

class QByteArray;
class QColor;
class QImage;
class QJsonDocument;
class QString;
struct DP_OnionSkins;

namespace net {

Message makeAnnotationCreateMessage(
	unsigned int contextId, uint16_t id, int32_t x, int32_t y, uint16_t w,
	uint16_t h);

Message makeAnnotationDeleteMessage(uint8_t contextId, uint16_t id);

Message makeAnnotationEditMessage(
	uint8_t contextId, uint16_t id, uint32_t bg, uint8_t flags, uint8_t border,
	const QString &text);

Message makeAnnotationReshapeMessage(
	uint8_t contextId, uint16_t id, int32_t x, int32_t y, uint16_t w,
	uint16_t h);

Message makeCanvasBackgroundMessage(uint8_t contextId, const QColor &color);

Message makeCanvasResizeMessage(
	uint8_t contextId, int32_t top, int32_t right, int32_t bottom,
	int32_t left);

Message makeDefaultLayerMessage(uint8_t contextId, uint16_t id);

Message makeFeatureAccessLevelsMessage(
	uint8_t contextId, int featureCount, const uint8_t *features);

Message makeFillRectMessage(
	uint8_t contextId, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QColor &color);

Message makeInternalCatchupMessage(uint8_t contextId, int progress);

Message makeInternalCleanupMessage(uint8_t contextId);

Message makeInternalResetMessage(uint8_t contextId);

Message makeInternalSnapshotMessage(uint8_t contextId);

Message makeKeyFrameSetMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex, uint16_t sourceId,
	uint16_t sourceIndex, uint8_t source);

Message makeKeyFrameLayerAttributesMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	const QVector<uint16_t> &layers);

Message makeKeyFrameRetitleMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	const QString &title);

Message makeKeyFrameDeleteMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	uint16_t moveTrackId, uint16_t moveFrameIndex);

Message
makeLaserTrailMessage(uint8_t contextId, uint32_t color, uint8_t persistence);

Message makeLayerAttributesMessage(
	uint8_t contextId, uint16_t id, uint8_t sublayer, uint8_t flags,
	uint8_t opacity, uint8_t blend);

Message makeLayerAclMessage(
	uint8_t contextId, uint16_t id, uint8_t flags,
	const QVector<uint8_t> &exclusive);

Message makeLayerCreateMessage(
	uint8_t contextId, uint16_t id, uint16_t source, uint32_t fill,
	uint8_t flags, const QString &name);

Message makeLayerTreeCreateMessage(
	uint8_t contextId, uint16_t id, uint16_t source, uint16_t target,
	uint32_t fill, uint8_t flags, const QString &name);

Message makeLayerDeleteMessage(uint8_t contextId, uint16_t id, bool merge);

Message
makeLayerTreeDeleteMessage(uint8_t contextId, uint16_t id, uint16_t mergeTo);

Message makeLayerTreeMoveMessage(
	uint8_t contextId, uint16_t layer, uint16_t parent, uint16_t sibling);

Message
makeLayerRetitleMessage(uint8_t contextId, uint16_t id, const QString &title);

Message makeMovePointerMessage(uint8_t contextId, int32_t x, int32_t y);

Message makeMoveRegionMessage(
	uint8_t contextId, uint16_t layer, int32_t bx, int32_t by, int32_t bw,
	int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3,
	int32_t y3, int32_t x4, int32_t y4, const QImage &mask);

Message makeMoveRectMessage(
	uint8_t contextId, uint16_t layer, uint16_t source, int32_t sx, int32_t sy,
	int32_t tx, int32_t ty, int32_t w, int32_t h, const QImage &mask);

Message makeTransformRegionMessage(
	uint8_t contextId, uint16_t layer, uint16_t source, int32_t bx, int32_t by,
	int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode,
	const QImage &mask);

Message makePutImageMessage(
	uint8_t contextId, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedImage);

Message
makeSetMetadataIntMessage(uint8_t contextId, uint8_t field, int32_t value);

Message makeTrackCreateMessage(
	uint8_t contextId, uint16_t id, uint16_t insertId, uint16_t sourceId,
	const QString &title);

Message makeTrackDeleteMessage(uint8_t contextId, uint16_t id);

Message
makeTrackOrderMessage(uint8_t contextId, const QVector<uint16_t> &tracks);

Message
makeTrackRetitleMessage(uint8_t contextId, uint16_t id, const QString &title);

Message makeUndoMessage(uint8_t contextId, uint8_t overrideUser, bool redo);

Message makeUndoDepthMessage(uint8_t contextId, uint8_t depth);

Message makeUndoPointMessage(uint8_t contextId);

Message makeUserAclMessage(uint8_t contextId, const QVector<uint8_t> &users);

Message makeUserInfoMessage(
	uint8_t contextId, uint8_t recipient, const QJsonDocument &msg);

// Fills given message list with put image messages, potentially cropping
// the image if the given coordinates are negative and splitting it into
// multiple messages if it doesn't fit into a single one.
void makePutImageMessages(
	MessageList &msgs, uint8_t contextId, uint16_t layer, uint8_t mode, int x,
	int y, const QImage &image);

Message makeLocalChangeLayerVisibilityMessage(int layerId, bool hidden);
Message makeLocalChangeBackgroundColorMessage(const QColor &color);
Message makeLocalChangeBackgroundClearMessage();
Message makeLocalChangeViewModeMessage(DP_ViewMode viewMode);
Message makeLocalChangeActiveLayerMessage(int layerId);
Message makeLocalChangeActiveFrameMessage(int frameIndex);
Message makeLocalChangeOnionSkinsMessage(const DP_OnionSkins *oss);
Message makeLocalChangeTrackVisibilityMessage(int trackId, bool hidden);
Message makeLocalChangeTrackOnionSkinMessage(int trackId, bool onionSkin);

Message makeMessageBackwardCompatible(const Message &msg);

}

#endif
