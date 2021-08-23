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

#include "canvasmodel.h"
#include "layerlist.h"
#include "userlist.h"
#include "aclfilter.h"
#include "selection.h"
#include "loader.h"
#include "paintengine.h"

#include "ora/orawriter.h"
#include "utils/identicon.h"
#include "net/internalmsg.h"

#include "../libshared/net/meta.h"
#include "../libshared/net/meta2.h"
#include "../libshared/net/recording.h"

#include "../rustpile/rustpile.h"

#include <QSettings>
#include <QDebug>
#include <QPainter>

namespace canvas {

void metaUserJoin(void *ctx, uint8_t user, uint8_t flags, const uint8_t *username, uintptr_t name_len, const uint8_t *avatarbytes, uintptr_t avatar_len);
void metaUserLeave(void *ctx, uint8_t user);
void metaChatMessage(void *ctx, uint8_t sender, uint8_t recipient, uint8_t tflags, uint8_t oflags, const uint8_t *message, uintptr_t message_len);
void metaLaserTrail(void *ctx, uint8_t user, uint8_t persistence, uint32_t color);
void metaMarkerMessage(void *ctx, uint8_t user, const uint8_t *message, uintptr_t message_len);
void metaDefaultLayer(void *ctx, uint16_t layerId);

CanvasModel::CanvasModel(uint8_t localUserId, QObject *parent)
	: QObject(parent), m_selection(nullptr), m_mode(Mode::Offline), m_localUserId(1)
{
	m_layerlist = new LayerListModel(this);
	m_userlist = new UserListModel(this);

	m_aclfilter = new AclFilter(this);

	connect(m_aclfilter, &AclFilter::operatorListChanged, m_userlist, &UserListModel::updateOperators);
	connect(m_aclfilter, &AclFilter::trustedUserListChanged, m_userlist, &UserListModel::updateTrustedUsers);
	connect(m_aclfilter, &AclFilter::userLocksChanged, m_userlist, &UserListModel::updateLocks);

	m_paintengine = new PaintEngine(this);

	rustpile::paintengine_register_meta_callbacks(
		m_paintengine->engine(),
		this,
		&metaUserJoin,
		&metaUserLeave,
		&metaChatMessage,
		&metaLaserTrail,
		&metaMarkerMessage,
		&metaDefaultLayer
	);

	m_aclfilter->reset(localUserId, true);

	m_layerlist->setMyId(localUserId);
	m_layerlist->setAclFilter(m_aclfilter);
	m_layerlist->setLayerGetter([this](int id)->const paintcore::Layer* {
#if 0 // FIXME
		return m_layerstack->getLayer(id);
#endif
		return nullptr;
	});

	connect(m_layerlist, &LayerListModel::autoSelectRequest, this, &CanvasModel::layerAutoselectRequest);
	connect(m_paintengine, &PaintEngine::resized, this, &CanvasModel::onCanvasResize);
	connect(m_paintengine, &PaintEngine::layersChanged, m_layerlist, &LayerListModel::setLayers, Qt::QueuedConnection); // queued connection needs to be set explicitly here for some reason

	updateLayerViewOptions();
}

uint8_t CanvasModel::localUserId() const
{
	return m_localUserId;
}

QSize CanvasModel::size() const
{
	return m_paintengine->size();
}

void CanvasModel::previewAnnotation(int id, const QRect &shape)
{
	emit previewAnnotationRequested(id, shape);
}

void CanvasModel::connectedToServer(uint8_t myUserId, bool join)
{
	Q_ASSERT(m_mode == Mode::Offline);
	if(myUserId == 0) {
		// Zero is a reserved "null" user ID
		qWarning("connectedToServer: local user ID is zero!");
	}

	m_localUserId = myUserId;
	m_layerlist->setMyId(myUserId);
	m_layerlist->setAutoselectAny(true);

	if(join)
		m_aclfilter->reset(myUserId, false);
	else
		m_aclfilter->setOnlineMode(myUserId);

	m_userlist->reset();
	m_mode = Mode::Online;
}

void CanvasModel::disconnectedFromServer()
{
	m_paintengine->cleanup();
	m_userlist->allLogout();
	m_aclfilter->reset(m_localUserId, true);
	m_mode = Mode::Offline;
}

void CanvasModel::startPlayback()
{
	Q_ASSERT(m_mode == Mode::Offline);
	m_mode = Mode::Playback;
#if 0 // FIXME
	m_statetracker->setShowAllUserMarkers(true);
#endif
}

void CanvasModel::endPlayback()
{
	Q_ASSERT(m_mode == Mode::Playback);
#if 0 // FIXME
	m_statetracker->setShowAllUserMarkers(false);
	m_statetracker->endPlayback();
#endif
}

void CanvasModel::handleCommand(protocol::MessagePtr cmd)
{
	using namespace protocol;

#if 0 // FIXME
	if(cmd->type() == protocol::MSG_INTERNAL) {
		m_statetracker->receiveQueuedCommand(cmd);
		return;
	}
#endif

	// Apply ACL filter
	if(m_mode != Mode::Playback && !m_aclfilter->filterMessage(*cmd)) {
		qWarning("Filtered %s message from %d", qPrintable(cmd->messageName()), cmd->contextId());
		if(m_recorder)
			m_recorder->recordMessage(cmd->asFiltered());
		return;

	} else if(m_recorder) {
		m_recorder->recordMessage(cmd);
	}

	// TODO use envelopes rather than MessagePtrs
	QByteArray buf(cmd->length(), 0);
	cmd->serialize(buf.data());
	m_paintengine->receiveMessages(false, buf);
	//m_statetracker->receiveQueuedCommand(cmd);
	emit canvasModified();
}

void CanvasModel::handleLocalCommand(protocol::MessagePtr cmd)
{
	m_layerlist->setAutoselectAny(false);
	QByteArray buf(cmd->length(), 0);
	cmd->serialize(buf.data());
	m_paintengine->receiveMessages(true, buf);
	emit canvasModified();
}

QImage CanvasModel::toImage(bool withBackground, bool withSublayers) const
{
	// TODO include annotations or not?
#if 0 // FIXME
	return m_layerstack->toFlatImage(false, withBackground, withSublayers);
#endif
	return QImage();
}

protocol::MessageList CanvasModel::generateSnapshot() const
{
#if 0 // FIXME
	auto loader = SnapshotLoader(m_statetracker->localId(), m_layerstack, m_aclfilter);
	loader.setDefaultLayer(m_layerlist->defaultLayer());
	loader.setPinnedMessage(m_pinnedMessage);
	return loader.loadInitCommands();
#endif
	return protocol::MessageList();
}

void CanvasModel::pickLayer(int x, int y)
{
#if 0 // FIXME
	const paintcore::Layer *l = m_layerstack->layerAt(x, y);
	if(l) {
		emit layerAutoselectRequest(l->id());
	}
#endif
}

void CanvasModel::pickColor(int x, int y, int layer, int diameter)
{
	const rustpile::Color c = rustpile::paintengine_sample_color(m_paintengine->engine(), x, y, layer, diameter);
	if(c.a > 0) {
		emit colorPicked(QColor::fromRgbF(c.r, c.g, c.b));
	}
}

void CanvasModel::inspectCanvas(int x, int y)
{
	int user = rustpile::paintengine_inspect_canvas(m_paintengine->engine(), x, y);
	emit canvasInspected(user);
}

void CanvasModel::inspectCanvas(int contextId)
{
	Q_ASSERT(contextId > 0 && contextId < 256);
	rustpile::paintengine_set_highlight_user(m_paintengine->engine(), contextId);
}

void CanvasModel::stopInspectingCanvas()
{
	rustpile::paintengine_set_highlight_user(m_paintengine->engine(), 0);
	emit canvasInspectionEnded();
}

void CanvasModel::setSelection(Selection *selection)
{
	if(m_selection != selection) {
		rustpile::paintengine_remove_preview(m_paintengine->engine(), 0);

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

void CanvasModel::updateLayerViewOptions()
{
	QSettings cfg;
	cfg.beginGroup("settings/animation");

	m_paintengine->setOnionskinOptions(
		cfg.value("onionskinsbelow", 4).toInt(),
		cfg.value("onionskinsabove", 4).toInt(),
		cfg.value("onionskintint", true).toBool()
	);
}

QImage CanvasModel::selectionToImage(int layerId) const
{
	if(m_selection && !m_selection->pasteImage().isNull()) {
		return m_selection->transformedPasteImage();
	}

	QRect rect { QPoint(), size() };
	if(m_selection)
		rect = rect.intersected(m_selection->boundingRect());

	QImage img(rect.size(), QImage::Format_ARGB32_Premultiplied);

	// Note: it's important to initialize the image content, as
	// get_layer_content will skip blank tiles
	img.fill(0);

	if(!rustpile::paintengine_get_layer_content(
		m_paintengine->engine(),
		layerId,
		rustpile::Rectangle { rect.x(), rect.y(), rect.width(), rect.height() },
		img.bits()
	)) {
		// Error (shouldn't happen unless there's a bug)
		qWarning("paintengine_get_layer_content failed!");
		img.fill(QColor(255, 0, 255));
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
#if 0 // FIXME
	m_layerstack->editor(0).reset();
	m_statetracker->reset();
	m_aclfilter->reset(m_statetracker->localId(), false);
#endif
}

void metaUserJoin(void *ctx, uint8_t user, uint8_t flags, const uint8_t *username, uintptr_t name_len, const uint8_t *avatarbytes, uintptr_t avatar_len)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	const QString name = QString::fromUtf8(reinterpret_cast<const char*>(username), name_len);

	QImage avatar;
	if(avatar_len == 0) {
		QByteArray avatarData = QByteArray::fromRawData(reinterpret_cast<const char*>(avatarbytes), avatar_len);
		if(!avatar.loadFromData(avatarData))
			qWarning("Avatar loading failed for user '%s' (#%d)", qPrintable(name), user);

		// Rescale avatar if its the wrong size
		if(avatar.width() > 32 || avatar.height() > 32) {
			avatar = avatar.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
	}
	if(avatar.isNull())
		avatar = make_identicon(name);

	const User u {
		user,
		name,
		QPixmap::fromImage(avatar),
		user == canvas->localUserId(),
		false,
		false,
		bool(flags & rustpile::JoinMessage_FLAGS_MOD),
		bool(flags & rustpile::JoinMessage_FLAGS_BOT),
		bool(flags & rustpile::JoinMessage_FLAGS_AUTH),
		false,
		false,
		true
	};

	canvas->m_userlist->userLogin(u);
	emit canvas->userJoined(user, name);
}

void metaUserLeave(void *ctx, uint8_t user)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	const QString name = canvas->m_userlist->getUsername(user);
	canvas->m_userlist->userLogout(user);
	emit canvas->userLeft(user, name);
}

void metaChatMessage(void *ctx, uint8_t sender, uint8_t recipient, uint8_t tflags, uint8_t oflags, const uint8_t *message, uintptr_t message_len)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	QString msg = QString::fromUtf8(reinterpret_cast<const char*>(message), message_len);

	if(oflags & rustpile::ChatMessage_OFLAGS_PIN) {
		if(msg == "-") // Special value to remove a pinned message
			msg = QString();

		if(canvas->m_pinnedMessage != msg) {
			canvas->m_pinnedMessage = msg;
			emit canvas->pinnedMessageChanged(msg);
		}
	} else {
		emit canvas->chatMessageReceived(sender, recipient, tflags, oflags, msg);
	}
}

void metaLaserTrail(void *ctx, uint8_t user, uint8_t persistence, uint32_t color)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	emit canvas->laserTrail(user, persistence, QColor::fromRgb(color));
}

void metaMarkerMessage(void *ctx, uint8_t user, const uint8_t *message, uintptr_t message_len)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	emit canvas->markerMessageReceived(user, QString::fromUtf8(reinterpret_cast<const char*>(message), message_len));
}

void metaDefaultLayer(void *ctx, uint16_t layerId)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	canvas->m_layerlist->setDefaultLayer(layerId);
}

}
