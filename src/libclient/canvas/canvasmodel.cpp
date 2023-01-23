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

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/userlist.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/canvas/acl.h"
#include "libclient/canvas/selection.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/net/envelopebuilder.h"

#include "libclient/utils/identicon.h"
#include "rustpile/rustpile.h"

#include <QSettings>
#include <QDebug>
#include <QPainter>

namespace canvas {

void metaUserJoin(void *ctx, uint8_t user, uint8_t flags, const uint8_t *username, uintptr_t name_len, const uint8_t *avatarbytes, uintptr_t avatar_len);
void metaUserLeave(void *ctx, uint8_t user);
void metaChatMessage(void *ctx, uint8_t sender, uint8_t recipient, uint8_t tflags, uint8_t oflags, const uint8_t *message, uintptr_t message_len);
void metaLaserTrail(void *ctx, uint8_t user, uint8_t persistence, uint32_t color);
void metaDefaultLayer(void *ctx, uint16_t layerId);
void metaAclChange(void *ctx, uint32_t changes);
void metaRecorderStateChanged(void *ctx, bool recording);

CanvasModel::CanvasModel(uint8_t localUserId, QObject *parent)
	: QObject(parent), m_selection(nullptr), m_localUserId(1)
{
	m_paintengine = new PaintEngine(this);

	m_aclstate = new AclState(this);
	m_layerlist = new LayerListModel(this);
	m_userlist = new UserListModel(this);
	m_timeline = new TimelineModel(this);
	m_metadata = new DocumentMetadata(m_paintengine, this);

	connect(m_aclstate, &AclState::userBitsChanged, m_userlist, &UserListModel::updateAclState);	

	rustpile::paintengine_register_meta_callbacks(
		m_paintengine->engine(),
		this,
		&metaUserJoin,
		&metaUserLeave,
		&metaChatMessage,
		&metaLaserTrail,
		&metaDefaultLayer,
		&metaAclChange,
		&metaRecorderStateChanged
	);

	m_aclstate->setLocalUserId(localUserId);
	rustpile::paintengine_reset_acl(m_paintengine->engine(), m_localUserId);

	m_layerlist->setAclState(m_aclstate);
	m_layerlist->setLayerGetter([this](int id)->QImage { return m_paintengine->getLayerImage(id); });

	connect(m_layerlist, &LayerListModel::autoSelectRequest, this, &CanvasModel::layerAutoselectRequest);
	connect(m_paintengine, &PaintEngine::resized, this, &CanvasModel::onCanvasResize);
	connect(m_paintengine, &PaintEngine::layersChanged, m_layerlist, &LayerListModel::setLayers, Qt::QueuedConnection); // queued connection needs to be set explicitly here for some reason
	connect(m_paintengine, &PaintEngine::layersChanged, m_timeline, &TimelineModel::setLayers, Qt::QueuedConnection);
	connect(m_paintengine, &PaintEngine::timelineChanged, m_timeline, [this]() {
		rustpile::paintengine_get_timeline(m_paintengine->engine(), m_timeline, timelineUpdateFrames);
	}, Qt::QueuedConnection);
	connect(m_paintengine, &PaintEngine::frameVisibilityChanged, m_layerlist, &LayerListModel::setLayersVisibleInFrame);

	updateLayerViewOptions();
}

bool CanvasModel::load(const QSize &size, const QColor &background)
{
	return rustpile::paintengine_load_blank(
		m_paintengine->engine(),
		size.width(),
		size.height(),
		rustpile::Color {
			float(background.redF()),
			float(background.greenF()),
			float(background.blueF()),
			float(background.alphaF())
		}
	);
}

rustpile::CanvasIoError CanvasModel::load(const QString &path)
{
	return rustpile::paintengine_load_file(m_paintengine->engine(), reinterpret_cast<const uint16_t*>(path.constData()), path.length());
}

rustpile::CanvasIoError CanvasModel::loadRecording(const QString &path)
{
	return rustpile::paintengine_load_recording(m_paintengine->engine(), reinterpret_cast<const uint16_t*>(path.constData()), path.length());
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
	if(myUserId == 0) {
		// Zero is a reserved "null" user ID
		qWarning("connectedToServer: local user ID is zero!");
	}

	m_localUserId = myUserId;
	m_layerlist->setAutoselectAny(true);

	m_aclstate->setLocalUserId(myUserId);

	if(join)
		rustpile::paintengine_reset_acl(m_paintengine->engine(), m_localUserId);

	m_userlist->reset();
}

void CanvasModel::disconnectedFromServer()
{
	m_paintengine->cleanup();
	m_userlist->allLogout();
	rustpile::paintengine_reset_acl(m_paintengine->engine(), m_localUserId);
	rustpile::paintengine_cleanup(m_paintengine->engine());
}

void CanvasModel::handleCommand(const net::Envelope &envelope)
{
	m_paintengine->receiveMessages(false, envelope);
	emit canvasModified();
}

void CanvasModel::handleLocalCommand(const net::Envelope &envelope)
{
	m_layerlist->setAutoselectAny(false);
	m_paintengine->receiveMessages(true, envelope);
}

net::Envelope CanvasModel::generateSnapshot() const
{
	net::EnvelopeBuilder eb;

	if(!m_pinnedMessage.isEmpty()) {
		rustpile::write_chat(eb, m_localUserId, 0, rustpile::ChatMessage_OFLAGS_PIN, reinterpret_cast<const uint16_t*>(m_pinnedMessage.constData()), m_pinnedMessage.length());
	}

	if(m_layerlist->defaultLayer() > 0)
		rustpile::write_defaultlayer(eb, 0, m_layerlist->defaultLayer());

	rustpile::paintengine_get_reset_snapshot(m_paintengine->engine(), eb);

	return eb.toEnvelope();
}

void CanvasModel::pickLayer(int x, int y)
{
	const uint16_t layer = rustpile::paintengine_pick_layer(m_paintengine->engine(), x, y);
	if(layer > 0)
		emit layerAutoselectRequest(layer);
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
	rustpile::paintengine_reset_canvas(m_paintengine->engine());
}

rustpile::CanvasIoError CanvasModel::startRecording(const QString &path)
{
	return rustpile::paintengine_start_recording(
		m_paintengine->engine(),
		reinterpret_cast<const uint16_t*>(path.constData()),
		path.length()
	);
}

void CanvasModel::stopRecording()
{
	rustpile::paintengine_stop_recording(m_paintengine->engine());
}

bool CanvasModel::isRecording() const
{
	return rustpile::paintengine_is_recording(m_paintengine->engine());
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

	emit canvas->laserTrail(user, qMin(15, int(persistence)) * 1000, QColor::fromRgb(color));
}

void metaDefaultLayer(void *ctx, uint16_t layerId)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	canvas->m_layerlist->setDefaultLayer(layerId);
}

void metaAclChange(void *ctx, uint32_t changes)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);

	if(changes & 0x01)
		canvas->m_aclstate->updateUserBits(*rustpile::paintengine_get_acl_users(canvas->paintEngine()->engine()));

	if(changes & 0x02)
		canvas->m_aclstate->updateLayers(canvas->paintEngine()->engine());

	if(changes & 0x04)
		canvas->m_aclstate->updateFeatures(*rustpile::paintengine_get_acl_features(canvas->paintEngine()->engine()));
}

void metaRecorderStateChanged(void *ctx, bool recording)
{
	Q_ASSERT(ctx);
	CanvasModel *canvas = static_cast<CanvasModel*>(ctx);
	emit canvas->recorderStateChanged(recording);
}

}
