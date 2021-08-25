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

#ifndef CANVASMODEL_H
#define CANVASMODEL_H

#include <QObject>
#include <QPointer>

namespace net {
	class Envelope;
}

namespace canvas {

class AclState;
class UserListModel;
class LayerListModel;
class Selection;
class PaintEngine;

class CanvasModel : public QObject
{
	Q_OBJECT
public:
	explicit CanvasModel(uint8_t localUserId, QObject *parent=nullptr);

	PaintEngine *paintEngine() const { return m_paintengine; }

	QString title() const { return m_title; }
	void setTitle(const QString &title) { if(m_title!=title) { m_title = title; emit titleChanged(title); } }

	QString pinnedMessage() const { return m_pinnedMessage; }

	Selection *selection() const { return m_selection; }
	void setSelection(Selection *selection);

	QImage toImage(bool withBackground=true, bool withSublayers=false) const;

#if 0 // FIXME
	protocol::MessageList generateSnapshot() const;
#endif

	uint8_t localUserId() const { return m_localUserId; }

	QImage selectionToImage(int layerId) const;
	void pasteFromImage(const QImage &image, const QPoint &defaultPoint, bool forceDefault);

	void connectedToServer(uint8_t myUserId, bool join);
	void disconnectedFromServer();
	void startPlayback();
	void endPlayback();

	AclState *aclState() const { return m_aclstate; }
	UserListModel *userlist() const { return m_userlist; }
	LayerListModel *layerlist() const { return m_layerlist; }

	/**
	 * @brief Is the canvas in "online mode"?
	 *
	 * This mainly affects how certain access controls are checked.
	 *
	 * TODO remove this
	 */
	bool isOnline() const { return m_mode == Mode::Online; }

#if 0 // FIXME
	/**
	 * @brief Set the Writer to use for recording
	 */
	void setRecorder(recording::Writer *writer) { m_recorder = writer; }
#endif

	//! Size of the canvas
	QSize size() const;

	/**
	 * Request the view layer to preview a change to an annotation
	 *
	 * This is used to preview the change or creation of an annotation.
	 * If an annotation with the given ID does not exist yet, one will be created.
	 * The annotation only exists in the view layer and will thus be automatically erased
	 * or replaced when the actual change goes through.
	 */
	void previewAnnotation(int id, const QRect &shape);

public slots:
	//! Handle a meta/command message received from the server
	void handleCommand(const net::Envelope &cmd);

	//! Handle a local drawing command (will be put in the local fork)
	void handleLocalCommand(const net::Envelope &cmd);

	void resetCanvas();

	void pickLayer(int x, int y);
	void pickColor(int x, int y, int layer, int diameter=0);
	void inspectCanvas(int x, int y);
	void inspectCanvas(int contextId);
	void stopInspectingCanvas();

	void updateLayerViewOptions();

signals:
	void layerAutoselectRequest(int id);
	void canvasModified();
	void selectionChanged(Selection *selection);
	void selectionRemoved();

	void previewAnnotationRequested(int id, const QRect &shape);

	void titleChanged(QString title);
	void pinnedMessageChanged(QString message);
	void imageSizeChanged();

	void colorPicked(const QColor &color);
	void canvasInspected(int lastEditedBy);
	void canvasInspectionEnded();

	void chatMessageReceived(int sender, int recipient, uint8_t tflags, uint8_t oflags, const QString &message);
	void markerMessageReceived(int id, const QString &message);

	void laserTrail(uint8_t userId, int persistence, const QColor &color);

	void userJoined(int id, const QString &name);
	void userLeft(int id, const QString &name);

	void canvasLocked(bool locked);

private slots:
	void onCanvasResize(int xoffset, int yoffset, const QSize &oldsize);

private:
	friend void metaUserJoin(void *canvas, uint8_t user, uint8_t flags, const uint8_t *name, uintptr_t name_len, const uint8_t *avatar, uintptr_t avatar_len);
	friend void metaUserLeave(void *ctx, uint8_t user);
	friend void metaChatMessage(void *ctx, uint8_t sender, uint8_t recipient, uint8_t tflags, uint8_t oflags, const uint8_t *message, uintptr_t message_len);
	friend void metaLaserTrail(void *ctx, uint8_t user, uint8_t persistence, uint32_t color);
	friend void metaMarkerMessage(void *ctx, uint8_t user, const uint8_t *message, uintptr_t message_len);
	friend void metaDefaultLayer(void *ctx, uint16_t layerId);
	friend void metaAclChange(void *ctx, uint32_t changes);

	AclState *m_aclstate;
	UserListModel *m_userlist;
	LayerListModel *m_layerlist;

	PaintEngine *m_paintengine;
	Selection *m_selection;

	QString m_title;
	QString m_pinnedMessage;

	enum class Mode { Offline, Online, Playback } m_mode;

	uint8_t m_localUserId;
};

}

#endif // CANVASSTATE_H
