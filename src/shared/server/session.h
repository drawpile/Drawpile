/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef DP_SHARED_SERVER_SESSION_H
#define DP_SHARED_SERVER_SESSION_H

#include <QVector>
#include <QHash>
#include <QString>

#include "../util/idlist.h"
#include "../net/message.h"

namespace protocol {
	class ToolChange;
	class PenMove;
	class PenUp;
	class LayerCreate;
	class LayerOrder;
	class LayerACL;
	class AnnotationCreate;
	class SessionConf;
}

namespace server {

struct LayerState {
	LayerState() : id(0), locked(false) {}
	LayerState(int id) : id(id), locked(false) {}

	int id;
	bool locked;
	QList<uint8_t> exclusive;
};

struct DrawingContext {
	DrawingContext() : currentLayer(0), penup(true) {}

	int currentLayer;
	bool penup;
};

/**
 * The serverside session state.
 */
struct SessionState {
	SessionState() : layerids(255), annotationids(255), userids(255), minorVersion(0),
		locked(false), closed(false), maxusers(255) { }

	//! Used layer IDs
	UsedIdList layerids;

	//! Used annotation IDs
	UsedIdList annotationids;

	//! Layer states
	QVector<LayerState> layers;

	//! Drawing context states
	QHash<int, DrawingContext> drawingctx;

	//! Used user/drawing context IDs
	UsedIdList userids;

	//! The client version used in this session
	int minorVersion;

	//! General all layer/all user lock
	bool locked;

	//! Is the session closed to new users?
	bool closed;

	//! Maximum number of users allowed in the session
	int maxusers;

	//! Lock new users by default
	bool lockdefault;

	//! If set, the session is password protected
	QString password;

	const LayerState *getLayerById(int id);

	/**
	 * @brief Set up the initial state based on the hosting users snapshot
	 *
	 * This is called only once
	 * @param messages
	 */
	void syncInitialState(const QList<protocol::MessagePtr> &messages);

	/**
	 * @brief Add an new layer to the list
	 * @param cmd layer creation command (will be updated with the new ID)
	 * @param assign if true, assign an ID for the layer
	 */
	void createLayer(protocol::LayerCreate &cmd, bool assign);

	/**
	 * @brief Reorder layers
	 *
	 * Layer list may be incomplete. The messag will be updated
	 * with the complete layer list.
	 * @param cmd
	 */
	void reorderLayers(protocol::LayerOrder &cmd);

	/**
	 * @brief Delete the layer with the given ID
	 * @param id
	 * @return true if layer existed
	 */
	bool deleteLayer(int id);

	/**
	 * @brief Update layer access control list
	 * @param cmd
	 * @return true if layer existed
	 */
	bool updateLayerAcl(const protocol::LayerACL &cmd);

	/**
	 * @brief Add a new annotation
	 *
	 * This just reserves the ID for the annotation
	 * @param assign if true, assign a new ID for the annotation, otherwise reserve the given one.
	 */
	void createAnnotation(protocol::AnnotationCreate &cmd, bool assign);

	/**
	 * @brief Delete an annotation
	 * @param id
	 * @return true if annotation was deleted
	 */
	bool deleteAnnotation(int id);

	/**
	 * @brief Set the configurable options of the session
	 * @param cmd
	 */
	void setSessionConfig(protocol::SessionConf &cmd);

	/**
	 * @brief Update a drawing context's layer
	 * @param cmd
	 */
	void drawingContextToolChange(const protocol::ToolChange &cmd);

	void drawingContextPenDown(const protocol::PenMove &cmd);

	void drawingContextPenUp(const protocol::PenUp &cmd);

	/**
	 * @brief Get a SessionConf message describing the current session options
	 * @return
	 */
	protocol::MessagePtr sessionConf() const;
};

}

#endif

