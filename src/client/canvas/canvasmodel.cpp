/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "net/client.h"
#include "net/layerlist.h"
#include "core/layerstack.h"
#include "core/annotationmodel.h"
#include "core/layer.h"
#include "ora/orawriter.h"
#include "loader.h"

#include "../shared/net/layer.h"

#include <QSettings>
#include <QDebug>
#include <QPainter>

namespace canvas {

CanvasModel::CanvasModel(net::Client *client, QObject *parent)
	: QObject(parent), m_selection(nullptr)
{
	Q_ASSERT(client);

	m_layerlist = client->layerlist();
	m_layerstack = new paintcore::LayerStack(this);
	m_statetracker = new StateTracker(m_layerstack, m_layerlist, client->myId(), this);
	m_usercursors = new UserCursorModel(this);
	m_lasers = new LaserTrailModel(this);

	connect(client, &net::Client::sessionTitleChange, this, &CanvasModel::setTitle);
	connect(client, &net::Client::drawingCommandReceived, this, &CanvasModel::handleDrawingCommand);
	connect(client, &net::Client::drawingCommandLocal, this, &CanvasModel::handleLocalCommand);

	connect(client, &net::Client::userJoined, m_usercursors, &UserCursorModel::setCursorName);
	connect(client, &net::Client::userPointerMoved, m_usercursors, &UserCursorModel::setCursorPosition);
	connect(client, &net::Client::userPointerMoved, m_lasers, &LaserTrailModel::cursorMove);

	connect(m_statetracker, &StateTracker::layerAutoselectRequest, this, &CanvasModel::layerAutoselectRequest);
	connect(client, &net::Client::layerVisibilityChange, m_layerstack, &paintcore::LayerStack::setLayerHidden);

	connect(m_statetracker, &StateTracker::userMarkerAttribs, m_usercursors, &UserCursorModel::setCursorAttributes);
	connect(m_statetracker, &StateTracker::userMarkerMove, m_usercursors, &UserCursorModel::setCursorPosition);
	connect(m_statetracker, &StateTracker::userMarkerHide, m_usercursors, &UserCursorModel::hideCursor);

	connect(m_layerstack, &paintcore::LayerStack::resized, this, &CanvasModel::onCanvasResize);
}

void CanvasModel::handleDrawingCommand(protocol::MessagePtr cmd)
{
	m_statetracker->receiveQueuedCommand(cmd);
	emit canvasModified();
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
	return m_layerstack->layers() > 1 || !m_layerstack->annotations()->isEmpty();
}

bool CanvasModel::save(const QString &filename) const
{
	if(filename.endsWith(".ora", Qt::CaseInsensitive)) {
		// Special case: Save as OpenRaster with all the layers intact.
		return openraster::saveOpenRaster(filename, m_layerstack);

	} else {
		// Regular image formats: flatten the image first.
		return toImage().save(filename);
	}
}

QList<protocol::MessagePtr> CanvasModel::generateSnapshot(bool forceNew) const
{
	QList<protocol::MessagePtr> snapshot;

	if(!m_statetracker->hasFullHistory() || forceNew) {
		// Generate snapshot
		snapshot = SnapshotLoader(this).loadInitCommands();

	} else {
		// Message stream contains (starts with) a snapshot: use it
		snapshot = m_statetracker->getHistory().toList();

		// Add layer ACL status
		// This is for the initial session snapshot. For new snapshots the
		// server will add the correct layer ACLs.
		for(const net::LayerListItem &layer : m_layerlist->getLayers()) {
			if(layer.isLockedFor(m_statetracker->localId()))
				snapshot << protocol::MessagePtr(new protocol::LayerACL(m_statetracker->localId(), layer.id, true, QList<uint8_t>()));
		}
	}

	return snapshot;
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
	m_layerstack->setViewBackgroundLayer(cfg.value("backgroundlayer", true).toBool());
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
			mp.drawImage(0, 0, m_selection->shapeMask(Qt::white));
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

}
