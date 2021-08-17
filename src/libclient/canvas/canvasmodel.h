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

#include "../libshared/net/message.h"
#include "../libshared/record/writer.h"

#include <QObject>
#include <QPointer>

namespace protocol {
	class UserJoin;
	class UserLeave;
	class LaserTrail;
	class MovePointer;
	class Chat;
	class Marker;
	class DefaultLayer;
}

namespace canvas {

class StateTracker;
class AclFilter;
class UserListModel;
class LayerListModel;
class UserCursorModel;
class LaserTrailModel;
class Selection;
class PaintEngine;

class CanvasModel : public QObject
{
	Q_PROPERTY(UserCursorModel* userCursors READ userCursors CONSTANT)
	Q_PROPERTY(LaserTrailModel* laserTrails READ laserTrails CONSTANT)
	Q_PROPERTY(Selection* selection READ selection WRITE setSelection NOTIFY selectionChanged)

	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
	Q_PROPERTY(QString pinnedMessage READ pinnedMessage NOTIFY pinnedMessageChanged)

	Q_OBJECT

public:
	explicit CanvasModel(uint8_t localUserId, QObject *parent=nullptr);

	PaintEngine *paintEngine() const { return m_paintengine; }
	UserCursorModel *userCursors() const { return m_usercursors; }
	LaserTrailModel *laserTrails() const { return m_lasers; }

	QString title() const { return m_title; }
	void setTitle(const QString &title) { if(m_title!=title) { m_title = title; emit titleChanged(title); } }

	QString pinnedMessage() const { return m_pinnedMessage; }

	Selection *selection() const { return m_selection; }
	void setSelection(Selection *selection);

	bool needsOpenRaster() const;
	QImage toImage(bool withBackground=true, bool withSublayers=false) const;

	protocol::MessageList generateSnapshot() const;

	uint8_t localUserId() const;

	QImage selectionToImage(int layerId) const;
	void pasteFromImage(const QImage &image, const QPoint &defaultPoint, bool forceDefault);

	void connectedToServer(uint8_t myUserId, bool join);
	void disconnectedFromServer();
	void startPlayback();
	void endPlayback();

	AclFilter *aclFilter() const { return m_aclfilter; }
	UserListModel *userlist() const { return m_userlist; }
	LayerListModel *layerlist() const { return m_layerlist; }

	/**
	 * @brief Is the canvas in "online mode"?
	 *
	 * This mainly affects how certain access controls are checked.
	 */
	bool isOnline() const { return m_mode == Mode::Online; }

	/**
	 * @brief Set the Writer to use for recording
	 */
	void setRecorder(recording::Writer *writer) { m_recorder = writer; }

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
	void handleCommand(protocol::MessagePtr cmd);

	//! Handle a local drawing command (will be put in the local fork)
	void handleLocalCommand(protocol::MessagePtr cmd);

	void resetCanvas();

	void pickLayer(int x, int y);
	void pickColor(int x, int y, int layer, int diameter=0);
	void inspectCanvas(int x, int y);
	void inspectCanvas(int contextId);
	void stopInspectingCanvas();

	void setLayerViewMode(int mode);
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
	void canvasInspected(int tx, int ty, int lastEditedBy);
	void canvasInspectionEnded();

	void chatMessageReceived(const protocol::MessagePtr &msg);
	void markerMessageReceived(int id, const QString &message);

	void userJoined(int id, const QString &name);
	void userLeft(int id, const QString &name);

	void canvasLocked(bool locked);

private slots:
	void onCanvasResize(int xoffset, int yoffset, const QSize &oldsize);

private:
	void metaUserJoin(const protocol::UserJoin &msg);
	void metaUserLeave(const protocol::UserLeave &msg);
	void metaChatMessage(protocol::MessagePtr msg);
	void metaLaserTrail(const protocol::LaserTrail &msg);
	void metaMovePointer(const protocol::MovePointer &msg);
	void metaMarkerMessage(const protocol::Marker &msg);
	void metaDefaultLayer(const protocol::DefaultLayer &msg);
	void metaSoftReset(uint8_t resetterId);

	AclFilter *m_aclfilter;
	UserListModel *m_userlist;
	LayerListModel *m_layerlist;

	PaintEngine *m_paintengine;
	UserCursorModel *m_usercursors;
	LaserTrailModel *m_lasers;
	Selection *m_selection;

	QPointer<recording::Writer> m_recorder;

	QString m_title;
	QString m_pinnedMessage;

	uint8_t m_localUserId;

	enum class Mode { Offline, Online, Playback } m_mode;
};

}

#endif // CANVASSTATE_H
