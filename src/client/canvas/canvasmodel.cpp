/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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
#include "usercursormodel.h"
#include "lasertrailmodel.h"
#include "statetracker.h"
#include "layerlist.h"
#include "userlist.h"
#include "aclfilter.h"
#include "loader.h"

#include "core/layerstack.h"
#include "core/annotationmodel.h"
#include "core/layer.h"
#include "ora/orawriter.h"

#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/recording.h"

#include <QSettings>
#include <QDebug>
#include <QPainter>

namespace canvas {

CanvasModel::CanvasModel(int localUserId, QObject *parent)
	: QObject(parent), m_selection(nullptr), m_mode(Mode::Offline)
{
	m_layerlist = new LayerListModel(this);
	m_userlist = new UserListModel(this);

	m_aclfilter = new AclFilter(this);

	connect(m_aclfilter, &AclFilter::operatorListChanged, m_userlist, &UserListModel::updateOperators);
	connect(m_aclfilter, &AclFilter::trustedUserListChanged, m_userlist, &UserListModel::updateTrustedUsers);
	connect(m_aclfilter, &AclFilter::userLocksChanged, m_userlist, &UserListModel::updateLocks);
	connect(m_aclfilter, &AclFilter::layerAclChange, m_layerlist, &LayerListModel::updateLayerAcl);

	m_layerstack = new paintcore::LayerStack(this);
	m_statetracker = new StateTracker(m_layerstack, m_layerlist, localUserId, this);
	m_usercursors = new UserCursorModel(this);
	m_lasers = new LaserTrailModel(this);

	m_aclfilter->reset(localUserId, true);

	m_layerlist->setMyId(localUserId);
	m_layerlist->setLayerGetter([this](int id)->paintcore::Layer* {
		return m_layerstack->getLayer(id);
	});

	m_usercursors->setLayerList(m_layerlist);

	connect(m_statetracker, &StateTracker::layerAutoselectRequest, this, &CanvasModel::layerAutoselectRequest);

	connect(m_statetracker, &StateTracker::userMarkerMove, m_usercursors, &UserCursorModel::setCursorPosition);
	connect(m_statetracker, &StateTracker::userMarkerHide, m_usercursors, &UserCursorModel::hideCursor);

	connect(m_layerstack, &paintcore::LayerStack::resized, this, &CanvasModel::onCanvasResize);
}

int CanvasModel::localUserId() const
{
	return m_statetracker->localId();
}

void CanvasModel::connectedToServer(int myUserId)
{
	Q_ASSERT(m_mode == Mode::Offline);
	m_layerlist->setMyId(myUserId);
	m_layerlist->unlockAll();
	m_statetracker->setLocalId(myUserId);
	m_aclfilter->reset(myUserId, false);
	m_mode = Mode::Online;
}

void CanvasModel::disconnectedFromServer()
{
	Q_ASSERT(m_mode == Mode::Online);
	m_statetracker->endRemoteContexts();
	m_userlist->clearUsers();
	m_layerlist->unlockAll();
	m_aclfilter->reset(m_statetracker->localId(), true);
	m_mode = Mode::Offline;
}

void CanvasModel::startPlayback()
{
	Q_ASSERT(m_mode == Mode::Offline);
	m_mode = Mode::Playback;
	m_statetracker->setShowAllUserMarkers(true);
}

void CanvasModel::endPlayback()
{
	Q_ASSERT(m_mode == Mode::Playback);
	m_statetracker->setShowAllUserMarkers(false);
	m_statetracker->endPlayback();
}

void CanvasModel::handleCommand(protocol::MessagePtr cmd)
{
	using namespace protocol;

	if(cmd->type() == protocol::MSG_INTERNAL) {
		m_statetracker->receiveQueuedCommand(cmd);
		return;
	}

	// Apply ACL filter
	if(m_mode != Mode::Playback && !m_aclfilter->filterMessage(*cmd)) {
		qWarning("Filtered %s message from %d", qPrintable(cmd->messageName()), cmd->contextId());
		if(m_recorder)
			m_recorder->recordMessage(cmd->asFiltered());
		return;

	} else if(m_recorder) {
		m_recorder->recordMessage(cmd);
	}

	if(cmd->isMeta()) {
		// Handle meta commands here
		switch(cmd->type()) {
		case MSG_CHAT:
			metaChatMessage(cmd);
			break;
		case MSG_USER_JOIN:
			metaUserJoin(cmd.cast<UserJoin>());
			break;
		case MSG_USER_LEAVE:
			metaUserLeave(cmd.cast<UserLeave>());
			break;
		case MSG_SESSION_OWNER:
		case MSG_TRUSTED_USERS:
		case MSG_USER_ACL:
		case MSG_SESSION_ACL:
		case MSG_LAYER_ACL:
			// Handled by the ACL filter
			break;
		case MSG_INTERVAL:
		case MSG_FILTERED:
			// recording playback related messages
			break;
		case MSG_LASERTRAIL:
			metaLaserTrail(cmd.cast<protocol::LaserTrail>());
			break;
		case MSG_MOVEPOINTER:
			metaMovePointer(cmd.cast<MovePointer>());
			break;
		case MSG_MARKER:
			metaMarkerMessage(cmd.cast<Marker>());
			break;
		case MSG_LAYER_DEFAULT:
			metaDefaultLayer(cmd.cast<DefaultLayer>());
			break;
		default:
			qWarning("Unhandled meta message %s", qPrintable(cmd->messageName()));
		}

	} else if(cmd->isCommand()) {
		// The state tracker handles all drawing commands
		m_statetracker->receiveQueuedCommand(cmd);
		emit canvasModified();

	} else {
		qWarning("CanvasModel::handleDrawingCommand: command %d is neither Meta nor Command type!", cmd->type());
	}
}

void CanvasModel::handleLocalCommand(protocol::MessagePtr cmd)
{
	m_statetracker->localCommand(cmd);
	emit canvasModified();
}

QImage CanvasModel::toImage() const
{
	// TODO include annotations or not?
	return m_layerstack->toFlatImage(false);
}

bool CanvasModel::needsOpenRaster() const
{
	return m_layerstack->layerCount() > 1 || !m_layerstack->annotations()->isEmpty();
}

QList<protocol::MessagePtr> CanvasModel::generateSnapshot(bool forceNew) const
{
	QList<protocol::MessagePtr> snapshot;

	if(!m_statetracker->hasFullHistory() || forceNew) {
		// Generate snapshot
		snapshot = SnapshotLoader(m_statetracker->localId(), m_layerstack, m_layerlist->getLayers(), this).loadInitCommands();

	} else {
		// Message stream contains (starts with) a snapshot: use it
		snapshot = m_statetracker->getHistory().toList();

		// Add default layer selection
		if(m_layerlist->defaultLayer() > 0)
			snapshot.prepend(protocol::MessagePtr(new protocol::DefaultLayer(m_statetracker->localId(), m_layerlist->defaultLayer())));

		// Add layer ACL status
		for(const LayerListItem &layer : m_layerlist->getLayers()) {
			if(layer.isLockedFor(m_statetracker->localId()))
				snapshot << protocol::MessagePtr(new protocol::LayerACL(m_statetracker->localId(), layer.id, true, QList<uint8_t>()));
		}
	}

	return snapshot;
}

void CanvasModel::pickLayer(int x, int y)
{
	const paintcore::Layer *l = m_layerstack->layerAt(x, y);
	if(l) {
		emit layerAutoselectRequest(l->id());
	}
}

void CanvasModel::pickColor(int x, int y, int layer, int diameter)
{
	QColor color;
	if(layer>0) {
		const paintcore::Layer *l = m_layerstack->getLayer(layer);
		if(layer)
			color = l->colorAt(x, y, diameter);
	} else {
		color = m_layerstack->colorAt(x, y, diameter);
	}

	if(color.isValid() && color.alpha()>0) {
		color.setAlpha(255);
		emit colorPicked(color);
	}
}

void CanvasModel::setLayerViewMode(int mode)
{
	m_layerstack->setViewMode(paintcore::LayerStack::ViewMode(mode));
	updateLayerViewOptions();
}

void CanvasModel::setSelection(Selection *selection)
{
	if(m_selection != selection) {
		m_layerstack->removePreviews();

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
	m_layerstack->setOnionskinMode(
		cfg.value("onionskinsbelow", 4).toInt(),
		cfg.value("onionskinsabove", 4).toInt(),
		cfg.value("onionskintint", true).toBool()
	);
}

/**
 * @brief Find an unused annotation ID
 *
 * Find an annotation ID (for this user) that is currently not in use.
 * @return available ID or 0 if none found
 */
int CanvasModel::getAvailableAnnotationId() const
{
	const int prefix = m_statetracker->localId() << 8;
	QList<int> takenIds;
	for(const paintcore::Annotation &a : m_layerstack->annotations()->getAnnotations()) {
		if((a.id & 0xff00) == prefix)
				takenIds << a.id;
	}

	for(int i=0;i<256;++i) {
		int id = prefix | i;
		if(!takenIds.contains(id))
			return id;
	}

	return 0;
}

QImage CanvasModel::selectionToImage(int layerId) const
{
	QImage img;

	paintcore::Layer *layer = m_layerstack->getLayer(layerId);
	if(layer)
		img = layer->toImage();
	else
		img = toImage();


	if(m_selection) {
		img = img.copy(m_selection->boundingRect().intersected(QRect(0, 0, img.width(), img.height())));

		if(!m_selection->isAxisAlignedRectangle()) {
			// Mask out pixels outside the selection
			QPainter mp(&img);
			mp.setCompositionMode(QPainter::CompositionMode_DestinationIn);

			QRect maskBounds;
			const QImage mask = m_selection->shapeMask(Qt::white, &maskBounds);

			mp.drawImage(qMin(0, maskBounds.left()), qMin(0, maskBounds.top()), mask);
		}
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
	m_layerlist->unlockAll();
	m_layerstack->reset();
	m_statetracker->reset();
	m_aclfilter->reset(m_statetracker->localId(), false);
}

void CanvasModel::metaUserJoin(const protocol::UserJoin &msg)
{
	User u(msg.contextId(), msg.name(), msg.contextId() == m_statetracker->localId(), msg.isAuthenticated(), msg.isModerator());
	if(m_aclfilter->isLockedByDefault())
		u.isLocked = true;

	m_userlist->addUser(u);
	m_usercursors->setCursorName(msg.contextId(), msg.name());

	emit userJoined(msg.contextId(), msg.name());
}

void CanvasModel::metaUserLeave(const protocol::UserLeave &msg)
{
	const QString name = m_userlist->getUserById(msg.contextId()).name;
	m_userlist->removeUser(msg.contextId());
	emit userLeft(msg.contextId(), name);
}

void CanvasModel::metaChatMessage(protocol::MessagePtr msg)
{
	Q_ASSERT(msg->type() == protocol::MSG_CHAT);
	const protocol::Chat &chat = msg.cast<protocol::Chat>();
	if(chat.isPin()) {
		QString pm = chat.message();
		if(m_pinnedMessage != pm) {
			if(pm == "-") // special value to remove a pinned message
				pm = QString();
			m_pinnedMessage = pm;
			emit pinnedMessageChanged(pm);
		}
	}
	emit chatMessageReceived(msg);
}

void CanvasModel::metaLaserTrail(const protocol::LaserTrail &msg)
{
	m_lasers->startTrail(msg.contextId(), QColor::fromRgb(msg.color()), msg.persistence());
}

void CanvasModel::metaMovePointer(const protocol::MovePointer &msg)
{
	QPoint p(msg.x() / 4.0, msg.y() / 4.0);
	m_usercursors->setCursorPosition(msg.contextId(), 0, p);
	m_lasers->addPoint(msg.contextId(), p);
}

void CanvasModel::metaMarkerMessage(const protocol::Marker &msg)
{
	emit markerMessageReceived(msg.contextId(), msg.text());
}

void CanvasModel::metaDefaultLayer(const protocol::DefaultLayer &msg)
{
	m_layerlist->setDefaultLayer(msg.layer());
	if(!m_statetracker->hasParticipated())
		emit layerAutoselectRequest(msg.layer());
}


}
