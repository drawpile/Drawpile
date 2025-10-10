// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CANVASMODEL_H
#define CANVASMODEL_H
#include "libclient/canvas/reconnectstate.h"
#include "libclient/drawdance/paintengine.h"
#include "libclient/net/message.h"
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QPointer>

class QJsonObject;
struct DP_Player;

namespace drawdance {
class CanvasState;
class SelectionSet;
}

namespace libclient {
namespace settings {
class Settings;
}
}

// Qt5 is very picky about types passed to QMetaType::invokeMethod, namespaces
// confuse it and fail to invoke at runtime. So keep this at top-level.
struct CanvasModelSetReconnectStateHistoryParams {
	QPointer<canvas::ReconnectState> reconnectState;
	DP_CanvasHistoryReconnectState *chrs;
};

Q_DECLARE_METATYPE(CanvasModelSetReconnectStateHistoryParams)

namespace canvas {

class AclState;
class DocumentMetadata;
class LayerListModel;
class MakeReconnectStateContext;
class PaintEngine;
class ReconnectState;
class SelectionModel;
class TimelineModel;
class TransformModel;
class UserListModel;

class CanvasModel final : public QObject {
	Q_OBJECT
public:
	static constexpr uint8_t MAIN_SELECTION_ID = 1;

	CanvasModel(
		libclient::settings::Settings &settings, uint8_t localUserId,
		int canvasImplementation, int fps, int snapshotMaxCount,
		long long snapshotMinDelayMs, bool wantCanvasHistoryDump,
		QObject *parent = nullptr);

	PaintEngine *paintEngine() const { return m_paintengine; }

	//! Load an empty canvas
	void loadBlank(
		int undoDepthLimit, const QSize &size, const QColor &background,
		const QString &initialLayerName, const QString &initialTrackName);

	void loadCanvasState(
		int undoDepthLimit, const drawdance::CanvasState &canvasState);

	//! Prepare to start playback, takes ownership of the given player
	void loadPlayer(DP_Player *player);

	QString title() const { return m_title; }
	void setTitle(const QString &title)
	{
		if(m_title != title) {
			m_title = title;
			emit titleChanged(title);
		}
	}

	bool isDirty() const { return m_dirty; }
	void setDirty(bool dirty) { m_dirty = dirty; }

	QString pinnedMessage() const { return m_pinnedMessage; }

	net::MessageList generateSnapshot(
		bool includePinnedMessage, unsigned int aclIncludeFlags,
		int overrideUndoLimit = -1,
		const QHash<int, int> *overrideTiers = nullptr,
		const QHash<int, QHash<int, int>> *overrideLimits = nullptr) const;

	// Returns the number of messages prepended, the rest are appended.
	int amendSnapshotMetadata(
		net::MessageList &snapshot, bool includePinnedMessage,
		unsigned int aclIncludeFlags, int overrideUndoLimit = -1,
		const QHash<int, int> *overrideTiers = nullptr,
		const QHash<int, QHash<int, int>> *overrideLimits = nullptr) const;

	uint8_t localUserId() const { return m_localUserId; }
	bool isCompatibilityMode() const { return m_compatibilityMode; }

	QImage selectionToImage(int layerId, bool *outFound = nullptr) const;

	QRect getPasteBounds(
		const QSize &imageSize, const QPoint &defaultPoint, bool forceDefault);

	ReconnectState *makeReconnectState(
		QObject *parent, const QJsonObject &sessionConfig,
		const HistoryIndex &hi);

	void connectedToServer(
		uint8_t myUserId, bool join, bool compatibilityMode,
		ReconnectState *reconnectState);
	void disconnectedFromServer();

	AclState *aclState() const { return m_aclstate; }
	UserListModel *userlist() const { return m_userlist; }
	LayerListModel *layerlist() const { return m_layerlist; }
	TimelineModel *timeline() const { return m_timeline; }
	DocumentMetadata *metadata() const { return m_metadata; }
	SelectionModel *selection() const { return m_selection; }
	TransformModel *transform() const { return m_transform; }

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
	 * If an annotation with the given ID does not exist yet, one will be
	 * created. The annotation only exists in the view layer and will thus be
	 * automatically erased or replaced when the actual change goes through.
	 */
	void previewAnnotation(int id, const QRect &shape);

	/**
	 * Reset the canvas to a blank state, as if the client had just joined a
	 * session.
	 *
	 * This is used to prepare the canvas to receive session reset data.
	 */
	void resetCanvas();

	//! Handle a meta/command message received from the server
	void handleCommands(int count, const net::Message *msgs);

	//! Handle a local drawing command (will be put in the local fork)
	void handleLocalCommands(int count, const net::Message *msgs);

	bool checkPermission(int feature);

public slots:
	void pickLayer(int x, int y);
	QColor pickColor(int x, int y, int layer, int diameter = 0);
	void inspectCanvas(int x, int y, bool clobber, bool showTiles);
	void inspectCanvas(int contextId, bool showTiles);
	void stopInspectingCanvas();

	void setTransformInterpolation(int transformInterpolation);

signals:
	void layerAutoselectRequest(int id);
	void canvasModified();

	void previewAnnotationRequested(int id, const QRect &shape);

	void titleChanged(QString title);
	void pinnedMessageChanged(QString message);

	void colorPicked(const QColor &color);
	void canvasInspected(int lastEditedBy);

	void chatMessageReceived(
		int sender, int recipient, uint8_t tflags, uint8_t oflags,
		const QString &message);

	void laserTrail(int userId, int persistence, const QColor &color);

	void userJoined(int id, const QString &name);
	void userLeft(int id, const QString &name);

	void recorderStateChanged(bool recording);

	void compatibilityModeChanged(bool compatibilityMode);

	void permissionDenied(int feature);

private slots:
	void onLaserTrail(int userId, int persistence, uint32_t color);

	void setReconnectStateHistory(
		const CanvasModelSetReconnectStateHistoryParams &params);

private:
	struct MakeReconnectStateParams;

	void handleMetaMessages(int count, const net::Message *msgs);
	void handleJoin(const net::Message &msg);
	void handleLeave(const net::Message &msg);
	void handleChat(const net::Message &msg);
	void handlePrivateChat(const net::Message &msg);

	void updatePaintEngineTransform();

	static void reconnectStateMakeCallback(
		void *user, DP_CanvasHistoryReconnectState *chrs);

	AclState *m_aclstate;
	UserListModel *m_userlist;
	LayerListModel *m_layerlist;
	TimelineModel *m_timeline;
	DocumentMetadata *m_metadata;
	SelectionModel *m_selection;
	TransformModel *m_transform;

	PaintEngine *m_paintengine;
	int m_transformInterpolation = 0;

	QString m_title;
	QString m_pinnedMessage;

	uint8_t m_localUserId = 1;
	bool m_dirty = false;
	bool m_compatibilityMode = false;
};

}

#endif // CANVASSTATE_H
