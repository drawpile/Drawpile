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
struct DP_CanvasHistoryReconnectState;
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

Message makeCameraCreateMessage(
	uint8_t contextId, uint16_t id, uint16_t sourceId, const QString &title);

Message makeCameraDeleteMessage(uint8_t contextId, uint16_t id);

Message makeCanvasBackgroundMessage(uint8_t contextId, const QColor &color);

Message makeCanvasResizeMessage(
	uint8_t contextId, int32_t top, int32_t right, int32_t bottom,
	int32_t left);

Message makeDefaultLayerMessage(uint8_t contextId, uint32_t id);

Message makeFeatureAccessLevelsMessage(
	uint8_t contextId, const QVector<uint8_t> &features);

Message
makeFeatureLimitsMessage(uint8_t contextId, const QVector<int32_t> &limits);

Message makeFillRectMessage(
	uint8_t contextId, uint32_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QColor &color);

Message makeInternalCatchupMessage(uint8_t contextId, int progress);

Message makeInternalCleanupMessage(uint8_t contextId);

Message makeInternalPaintSyncMessage(
	uint8_t contextId, void (*callback)(void *), void *user);

Message makeInternalResetMessage(uint8_t contextId);

Message makeInternalReconnectStateApplyMessage(
	uint8_t contextId, DP_CanvasHistoryReconnectState *chrs);

Message makeInternalReconnectStateMakeMessage(
	uint8_t contextId,
	void (*callback)(void *, DP_CanvasHistoryReconnectState *), void *user);

Message makeInternalStreamResetStartMessage(
	uint8_t contextId, const QString &correlator);

Message makeInternalSnapshotMessage(uint8_t contextId);

Message makeKeyFrameSetMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex, uint32_t sourceId,
	uint16_t sourceIndex, uint8_t source);

Message makeKeyFrameLayerAttributesMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	const QVector<uint32_t> &layers);

Message makeKeyFrameRetitleMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	const QString &title);

Message makeKeyFrameDeleteMessage(
	uint8_t contextId, uint16_t trackId, uint16_t frameIndex,
	uint16_t moveTrackId, uint16_t moveFrameIndex);

Message
makeLaserTrailMessage(uint8_t contextId, uint32_t color, uint8_t persistence);

Message makeLayerAttributesMessage(
	uint8_t contextId, uint32_t id, uint8_t sublayer, uint8_t flags,
	uint8_t opacity, uint8_t blend);

Message makeLayerAclMessage(
	uint8_t contextId, uint32_t id, uint8_t flags,
	const QVector<uint8_t> &exclusive);

Message makeLayerTreeCreateMessage(
	uint8_t contextId, uint32_t id, uint32_t source, uint32_t target,
	uint32_t fill, uint8_t flags, const QString &name);

Message
makeLayerTreeDeleteMessage(uint8_t contextId, uint32_t id, uint32_t mergeTo);

Message makeLayerTreeMoveMessage(
	uint8_t contextId, uint32_t layer, uint32_t parent, uint32_t sibling);

Message
makeLayerRetitleMessage(uint8_t contextId, uint32_t id, const QString &title);

Message makeMovePointerMessage(uint8_t contextId, int32_t x, int32_t y);

Message makeMoveRectMessageCompat(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t sx, int32_t sy,
	int32_t tx, int32_t ty, int32_t w, int32_t h, uint8_t blend,
	uint8_t opacity, const QImage &mask, bool compatibilityMode);

Message makeTransformRegionMessageCompat(
	uint8_t contextId, uint32_t layer, uint32_t source, int32_t bx, int32_t by,
	int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
	int32_t x3, int32_t y3, int32_t x4, int32_t y4, uint8_t mode, uint8_t blend,
	uint8_t opacity, const QImage &mask, bool compatibilityMode);

Message makePutImageMessage(
	uint8_t contextId, uint32_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedImage);

Message makePutImageZstdMessage(
	uint8_t contextId, uint32_t layer, uint8_t mode, uint32_t x, uint32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedImage);

Message makeSelectionClearMessage(uint8_t contextId, uint8_t selectionId);

Message makeSelectionPutMessage(
	uint8_t contextId, uint8_t selectionId, uint8_t op, int32_t x, int32_t y,
	uint32_t w, uint32_t h, const QByteArray &compressedMask);

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
void makePutImageMessagesCompat(
	MessageList &msgs, uint8_t contextId, uint32_t layer, uint8_t mode, int x,
	int y, const QImage &image, bool compatibilityMode);

void makeSelectionPutMessages(
	MessageList &msgs, uint8_t contextId, uint8_t selectionId, uint8_t op,
	int x, int y, int w, int h, const QImage &image);

Message makeLocalChangeLayerVisibilityMessage(int layerId, bool hidden);
Message makeLocalChangeBackgroundColorMessage(const QColor &color);
Message makeLocalChangeBackgroundClearMessage();
Message makeLocalChangeViewModeMessage(DP_ViewMode viewMode);
Message makeLocalChangeActiveLayerMessage(int layerId);
Message makeLocalChangeActiveFrameMessage(int frameIndex);
Message makeLocalChangeOnionSkinsMessage(const DP_OnionSkins *oss);
Message makeLocalChangeTrackVisibilityMessage(int trackId, bool hidden);
Message makeLocalChangeTrackOnionSkinMessage(int trackId, bool onionSkin);
Message makeLocalChangeLayerSketchMessage(
	int layerId, uint16_t opacity, const QColor &tint);
Message makeLocalChangeLayerAlphaLockMessage(int layerId, bool alphaLock);
Message makeLocalChangeLayerCensoredMessage(int layerId, bool censored);

DP_Message *makeLocalMatchMessage(const Message &msg, bool disguiseAsPutImage);

}

#endif
