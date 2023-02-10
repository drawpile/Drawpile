/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

extern "C" {
#include <dpmsg/acl.h>
#include <dpmsg/message.h>
}

#include "canvasmodel.h"
#include "layerlist.h"
#include "userlist.h"
#include "timelinemodel.h"
#include "acl.h"
#include "selection.h"
#include "paintengine.h"
#include "documentmetadata.h"

#include "utils/identicon.h"

#include <QSettings>
#include <QDebug>
#include <QPainter>

namespace canvas {

CanvasModel::CanvasModel(uint8_t localUserId, int fps, int snapshotMaxCount,
		long long snapshotMinDelayMs, bool wantCanvasHistoryDump, QObject *parent)
	: QObject(parent), m_selection(nullptr), m_localUserId(1)
{
	m_paintengine = new PaintEngine(
		fps, snapshotMaxCount, snapshotMinDelayMs, wantCanvasHistoryDump, this);

	m_aclstate = new AclState(this);
	m_layerlist = new LayerListModel(this);
	m_userlist = new UserListModel(this);
	m_timeline = new TimelineModel(this);
	m_metadata = new DocumentMetadata(m_paintengine, this);

	connect(m_aclstate, &AclState::userBitsChanged, m_userlist, &UserListModel::updateAclState);
	connect(m_paintengine, &PaintEngine::aclsChanged, m_aclstate, &AclState::aclsChanged);
	connect(m_paintengine, &PaintEngine::laserTrail, this, &CanvasModel::onLaserTrail);
	connect(m_paintengine, &PaintEngine::defaultLayer, m_layerlist, &LayerListModel::setDefaultLayer);
	connect(m_paintengine, &PaintEngine::recorderStateChanged, this, &CanvasModel::recorderStateChanged);

	m_aclstate->setLocalUserId(localUserId);

	m_layerlist->setAclState(m_aclstate);
	m_layerlist->setLayerGetter([this](int id)->QImage { return m_paintengine->getLayerImage(id); });

	connect(m_layerlist, &LayerListModel::autoSelectRequest, this, &CanvasModel::layerAutoselectRequest);
	connect(m_paintengine, &PaintEngine::resized, this, &CanvasModel::onCanvasResize);
	connect(m_paintengine, &PaintEngine::layersChanged, m_layerlist, &LayerListModel::setLayers);
	connect(m_paintengine, &PaintEngine::layersChanged, m_timeline, &TimelineModel::setLayers);
	connect(m_paintengine, &PaintEngine::timelineChanged, m_timeline, &TimelineModel::setTimeline);
	connect(m_paintengine, &PaintEngine::frameVisibilityChanged, m_layerlist, &LayerListModel::setLayersVisibleInFrame);
}

void CanvasModel::loadBlank(const QSize &size, const QColor &background)
{
	m_paintengine->enqueueLoadBlank(size, background);
}

void CanvasModel::loadCanvasState(const drawdance::CanvasState &canvasState)
{
	m_paintengine->reset(m_localUserId, canvasState);
}

void CanvasModel::loadPlayer(DP_Player *player)
{
	return m_paintengine->reset(m_localUserId, drawdance::CanvasState::null(), player);
}

QSize CanvasModel::size() const
{
	return m_paintengine->viewCanvasState().size();
}

void CanvasModel::previewAnnotation(int id, const QRect &shape)
{
	emit previewAnnotationRequested(id, shape);
}

void CanvasModel::connectedToServer(uint8_t myUserId, bool join)
{
	if(myUserId == 0) {
		// Zero is a reserved "null" user ID
		qWarning("connectedToServer: local user ID is zero!");
	}

	m_localUserId = myUserId;
	m_layerlist->setAutoselectAny(true);

	m_aclstate->setLocalUserId(myUserId);

	if(join) {
		m_paintengine->resetAcl(m_localUserId);
	}

	m_userlist->reset();
}

void CanvasModel::disconnectedFromServer()
{
	m_paintengine->cleanup();
	m_userlist->allLogout();
	m_paintengine->resetAcl(m_localUserId);
	m_paintengine->cleanup();
}

void CanvasModel::handleCommands(int count, const drawdance::Message *msgs)
{
	handleMetaMessages(count, msgs);
	if(m_paintengine->receiveMessages(false, count, msgs) != 0) {
		emit canvasModified();
	}
}

void CanvasModel::handleLocalCommands(int count, const drawdance::Message *msgs)
{
	if(m_paintengine->receiveMessages(true, count, msgs) != 0) {
		m_layerlist->setAutoselectAny(false);
	}
}

void CanvasModel::handleMetaMessages(int count, const drawdance::Message *msgs)
{
	for (int i = 0; i < count; ++i) {
		const drawdance::Message &msg = msgs[i];
		switch(msg.type()) {
		case DP_MSG_JOIN:
			handleJoin(msg);
			break;
		case DP_MSG_LEAVE:
			handleLeave(msg);
			break;
		case DP_MSG_CHAT:
			handleChat(msg);
			break;
		case DP_MSG_PRIVATE_CHAT:
			handlePrivateChat(msg);
			break;
		default:
			break;
		}
	}
}

void CanvasModel::handleJoin(const drawdance::Message &msg)
{
	DP_MsgJoin *mj = DP_msg_join_cast(msg.get());
	uint8_t user_id = msg.contextId();

	size_t nameLength;
	const char *nameBytes = DP_msg_join_name(mj, &nameLength);
	QString name = QString::fromUtf8(nameBytes, nameLength);

	size_t avatarSize;
	const unsigned char *avatarBytes = DP_msg_join_avatar(mj, &avatarSize);
	QImage avatar;
	if(avatarSize != 0) {
		QByteArray avatarData = QByteArray::fromRawData(reinterpret_cast<const char*>(avatarBytes), avatarSize);
		if(!avatar.loadFromData(avatarData)) {
			qWarning("Avatar loading failed for user '%s' (#%d)", qPrintable(name), user_id);
		}

		// Rescale avatar if its the wrong size
		if(avatar.width() > 32 || avatar.height() > 32) {
			avatar = avatar.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
	}
	if(avatar.isNull()) {
		avatar = make_identicon(name);
	}

	uint8_t flags = DP_msg_join_flags(mj);
	const User u {
		user_id,
		name,
		QPixmap::fromImage(avatar),
		user_id == localUserId(),
		false,
		false,
		bool(flags & DP_MSG_JOIN_FLAGS_MOD),
		bool(flags & DP_MSG_JOIN_FLAGS_BOT),
		bool(flags & DP_MSG_JOIN_FLAGS_AUTH),
		false,
		false,
		true
	};

	m_userlist->userLogin(u);
	emit userJoined(user_id, name);
}

void CanvasModel::handleLeave(const drawdance::Message &msg)
{
	uint8_t user_id = msg.contextId();
	QString name = m_userlist->getUsername(user_id);
	m_userlist->userLogout(user_id);
	emit userLeft(user_id, name);
}

void CanvasModel::handleChat(const drawdance::Message &msg)
{
	DP_MsgChat *mc = DP_msg_chat_cast(msg.get());

	size_t messageLength;
	const char *messageBytes = DP_msg_chat_message(mc, &messageLength);
	QString message = QString::fromUtf8(messageBytes, messageLength);

	uint8_t oflags = DP_msg_chat_oflags(mc);
	if(oflags & DP_MSG_CHAT_OFLAGS_PIN) {
		if(!m_userlist->isOperator(msg.contextId())) {
			return; // Only operators can (un)pin messages.
		}

		// Special value to remove a pinned message
		if(message == "-") {
			message = QString();
		}

		if(m_pinnedMessage != message) {
			m_pinnedMessage = message;
			emit pinnedMessageChanged(message);
		}
	} else {
		uint8_t tflags = DP_msg_chat_tflags(mc);
		emit chatMessageReceived(msg.contextId(), 0, tflags, oflags, message);
	}
}

void CanvasModel::handlePrivateChat(const drawdance::Message &msg)
{
	DP_MsgPrivateChat *mpc = DP_msg_private_chat_cast(msg.get());

	size_t messageLength;
	const char *messageBytes = DP_msg_private_chat_message(mpc, &messageLength);
	QString message = QString::fromUtf8(messageBytes, messageLength);

	uint8_t target = DP_msg_private_chat_target(mpc);
	uint8_t oflags = DP_msg_private_chat_oflags(mpc);
	emit chatMessageReceived(msg.contextId(), target, 0, oflags, message);
}

void CanvasModel::onLaserTrail(uint8_t userId, int persistence, uint32_t color)
{
	emit laserTrail(userId, qMin(15, persistence) * 1000, QColor::fromRgb(color));
}

drawdance::MessageList CanvasModel::generateSnapshot() const
{
	drawdance::MessageList snapshot;
	m_paintengine->historyCanvasState().toResetImage(snapshot, 0);
	amendSnapshotMetadata(snapshot);
	return snapshot;
}

void CanvasModel::amendSnapshotMetadata(drawdance::MessageList &snapshot) const
{
	if(!m_pinnedMessage.isEmpty()) {
		snapshot.prepend(drawdance::Message::makeChat(
			m_localUserId, 0, DP_MSG_CHAT_OFLAGS_PIN, m_pinnedMessage));
	}

	int defaultLayerId = m_layerlist->defaultLayer();
	if(defaultLayerId > 0) {
		snapshot.append(drawdance::Message::makeDefaultLayer(0, defaultLayerId));
	}

	m_paintengine->aclState().toResetImage(snapshot, m_localUserId);
}

void CanvasModel::pickLayer(int x, int y)
{
	int layerId = m_paintengine->viewCanvasState().pickLayer(x, y);
	if(layerId > 0) {
		emit layerAutoselectRequest(layerId);
	}
}

void CanvasModel::pickColor(int x, int y, int layer, int diameter)
{
	QColor color = m_paintengine->sampleColor(x, y, layer, diameter);
	if(color.alpha() > 0) {
		color.setAlpha(255);
		emit colorPicked(color);
	}
}

void CanvasModel::inspectCanvas(int x, int y)
{
	unsigned int contextId = m_paintengine->viewCanvasState().pickContextId(x, y);
	if (contextId != 0) {
		inspectCanvas(contextId);
	}
	emit canvasInspected(contextId);
}

void CanvasModel::inspectCanvas(int contextId)
{
	Q_ASSERT(contextId > 0 && contextId < 256);
	m_paintengine->setInspectContextId(contextId);
}

void CanvasModel::stopInspectingCanvas()
{
	m_paintengine->setInspectContextId(0);
	emit canvasInspectionEnded();
}

void CanvasModel::setSelection(Selection *selection)
{
	if(m_selection != selection) {
		m_paintengine->clearPreview();

		const bool hadSelection = m_selection != nullptr;

		if(hadSelection && m_selection->parent() == this)
			m_selection->deleteLater();

		if(selection && !selection->parent())
			selection->setParent(this);

		m_selection = selection;

		emit selectionChanged(selection);
		if(hadSelection && !selection)
			emit selectionRemoved();
	}
}

QImage CanvasModel::selectionToImage(int layerId) const
{
	if(m_selection && !m_selection->pasteImage().isNull()) {
		return m_selection->transformedPasteImage();
	}

	drawdance::CanvasState canvasState = m_paintengine->viewCanvasState();
	QRect rect{QPoint(), canvasState.size()};
	if(m_selection) {
		rect = rect.intersected(m_selection->boundingRect());
	}
	if(rect.isEmpty()) {
		qWarning("selectionToImage: empty selection");
		return QImage{};
	}

	QImage img;
	if(layerId == 0) {
		img = canvasState.toFlatImage(true, true, &rect);
	} else {
		drawdance::LayerContent layerContent = canvasState.searchLayerContent(layerId);
		if(layerContent.isNull()) {
			qWarning("selectionToImage: layer %d not found", layerId);
			QImage img(rect.size(), QImage::Format_ARGB32_Premultiplied);
			img.fill(0);
			return img;
		}
		img = layerContent.toImage(rect);
	}

	if(m_selection && !m_selection->isAxisAlignedRectangle()) {
		// Mask out pixels outside the selection
		QPainter mp(&img);
		mp.setCompositionMode(QPainter::CompositionMode_DestinationIn);

		QRect maskBounds;
		const QImage mask = m_selection->shapeMask(Qt::white, &maskBounds);

		mp.drawImage(qMin(0, maskBounds.left()), qMin(0, maskBounds.top()), mask);
	}
	return img;
}

void CanvasModel::pasteFromImage(const QImage &image, const QPoint &defaultPoint, bool forceDefault)
{
	QPoint center;
	if(m_selection && !forceDefault)
		center = m_selection->boundingRect().center();
	else
		center = defaultPoint;

	Selection *paste = new Selection;
	paste->setShapeRect(QRect(center.x() - image.width()/2, center.y() - image.height()/2, image.width(), image.height()));
	paste->setPasteImage(image);

	setSelection(paste);
}

void CanvasModel::onCanvasResize(int xoffset, int yoffset, const QSize &oldsize)
{
	Q_UNUSED(oldsize);

	// Adjust selection when new space was added to the left or top side
	// so it remains visually in the same place
	if(m_selection) {
		if(xoffset || yoffset) {
			QPoint offset(xoffset, yoffset);
			m_selection->translate(offset);
		}
	}
}

void CanvasModel::resetCanvas()
{
	setTitle(QString());
	m_paintengine->enqueueReset();
}

drawdance::RecordStartResult CanvasModel::startRecording(const QString &path)
{
	return m_paintengine->startRecording(path);
}

bool CanvasModel::stopRecording()
{
	return m_paintengine->stopRecording();
}

bool CanvasModel::isRecording() const
{
	return m_paintengine->isRecording();
}

}
