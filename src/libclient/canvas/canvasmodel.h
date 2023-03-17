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

#include "libclient/drawdance/message.h"
#include "libclient/drawdance/paintengine.h"

#include <QObject>
#include <QPointer>

struct DP_Player;

namespace drawdance {
	class CanvasState;
}

namespace canvas {

class AclState;
class UserListModel;
class LayerListModel;
class TimelineModel;
class Selection;
class DocumentMetadata;
class PaintEngine;

class CanvasModel final : public QObject
{
	Q_OBJECT
public:
	CanvasModel(
		uint8_t localUserId, int fps, int snapshotMaxCount,
		long long snapshotMinDelayMs, bool wantCanvasHistoryDump,
		QObject *parent = nullptr);

	PaintEngine *paintEngine() const { return m_paintengine; }

	//! Load an empty canvas
	void loadBlank(const QSize &size, const QColor &background);

	void loadCanvasState(const drawdance::CanvasState &canvasState);

	//! Prepare to start playback, takes ownership of the given player
	void loadPlayer(DP_Player *player);

	QString title() const { return m_title; }
	void setTitle(const QString &title) { if(m_title!=title) { m_title = title; emit titleChanged(title); } }

	QString pinnedMessage() const { return m_pinnedMessage; }

	Selection *selection() const { return m_selection; }
	void setSelection(Selection *selection);

	drawdance::MessageList generateSnapshot() const;
	// Adds default layer and pinned message to a reset image.
	void amendSnapshotMetadata(drawdance::MessageList &snapshot) const;

	uint8_t localUserId() const { return m_localUserId; }

	QImage selectionToImage(int layerId) const;
	void pasteFromImage(const QImage &image, const QPoint &defaultPoint, bool forceDefault);

	void connectedToServer(uint8_t myUserId, bool join);
	void disconnectedFromServer();

	AclState *aclState() const { return m_aclstate; }
	UserListModel *userlist() const { return m_userlist; }
	LayerListModel *layerlist() const { return m_layerlist; }
	TimelineModel *timeline() const { return m_timeline; }
	DocumentMetadata *metadata() const { return m_metadata; }

	//! Open a recording file and start recording
	drawdance::RecordStartResult startRecording(const QString &path);

	//! Stop recording, returns false if none was in progress
	bool stopRecording();

	//! Is recording in progress?
	bool isRecording() const;

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

	/**
	 * Reset the canvas to a blank state, as if the client had just joined a session.
	 *
	 * This is used to prepare the canvas to receive session reset data.
	 */
	void resetCanvas();

public slots:
	//! Handle a meta/command message received from the server
	void handleCommands(int count, const drawdance::Message *msgs);

	//! Handle a local drawing command (will be put in the local fork)
	void handleLocalCommands(int count, const drawdance::Message *msgs);

	void pickLayer(int x, int y);
	void pickColor(int x, int y, int layer, int diameter=0);
	void inspectCanvas(int x, int y);
	void inspectCanvas(int contextId);
	void stopInspectingCanvas();

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

	void laserTrail(uint8_t userId, int persistence, const QColor &color);

	void userJoined(int id, const QString &name);
	void userLeft(int id, const QString &name);

	void recorderStateChanged(bool recording);

private slots:
	void onCanvasResize(int xoffset, int yoffset, const QSize &oldsize);
	void onLaserTrail(uint8_t userId, int persistence, uint32_t color);

private:
	void handleMetaMessages(int count, const drawdance::Message *msgs);
	void handleJoin(const drawdance::Message &msg);
	void handleLeave(const drawdance::Message &msg);
	void handleChat(const drawdance::Message &msg);
	void handlePrivateChat(const drawdance::Message &msg);

	AclState *m_aclstate;
	UserListModel *m_userlist;
	LayerListModel *m_layerlist;
	TimelineModel *m_timeline;
	DocumentMetadata *m_metadata;

	PaintEngine *m_paintengine;
	Selection *m_selection;

	QString m_title;
	QString m_pinnedMessage;

	uint8_t m_localUserId;
};

}

#endif // CANVASSTATE_H
