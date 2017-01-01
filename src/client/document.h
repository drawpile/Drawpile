/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2016 Calle Laakkonen

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

#ifndef DRAWPILE_DOCUMENT_H
#define DRAWPILE_DOCUMENT_H

#include <QObject>

#include "core/blendmodes.h"
#include "../shared/net/message.h" // TODO hide this

class QString;
class QTimer;

namespace canvas {
	class CanvasModel;
	class SessionLoader;
	class StateSavepoint;
}
namespace net { class Client; }
namespace recording { class Writer; }
namespace tools { class ToolController; }

/**
 * @brief An active document and its associated data, including the network connection
 *
 * This is an UI agnostic class that should be usable from both a widget
 * based application or a pure QML app.
 *
 */
class Document : public QObject
{
	Q_PROPERTY(canvas::CanvasModel* canvas READ canvas() NOTIFY canvasChanged)
	Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyCanvas)
	Q_PROPERTY(bool autosave READ isAutosave WRITE setAutosave NOTIFY autosaveChanged)
	Q_PROPERTY(bool canAutosave READ canAutosave NOTIFY canAutosaveChanged)
	Q_PROPERTY(QString sessionTitle READ sessionTitle NOTIFY sessionTitleChanged)
	Q_PROPERTY(QString currentFilename READ currentFilename() NOTIFY currentFilenameChanged)
	Q_PROPERTY(bool recording READ isRecording() NOTIFY recorderStateChanged)
	Q_PROPERTY(bool serverSpaceLow READ isServerSpaceLow NOTIFY serverSpaceLowChanged)

	Q_PROPERTY(bool sessionClosed READ isSessionClosed NOTIFY sessionClosedChanged)
	Q_PROPERTY(bool sessionPreserveChat READ isSessionPreserveChat NOTIFY sessionPreserveChatChanged)
	Q_PROPERTY(bool sessionPasswordProtected READ isSessionPasswordProtected NOTIFY sessionPasswordChanged)
	Q_OBJECT
public:
	explicit Document(QObject *parent = 0);
	~Document();

	QString title() const;

	canvas::CanvasModel *canvas() const { return m_canvas; }
	tools::ToolController *toolCtrl() const { return m_toolctrl; }
	net::Client *client() const { return m_client; }

	/**
	 * @brief (Re)initialize the canvas
	 *
	 * This deletes the old canvas (if it exists) and creates a fresh one.
	 */
	void initCanvas();

	/**
	 * @brief Initialize the canvas from a session loader
	 *
	 * In case of error, check the loader's error and warning messages.
	 *
	 * @param loader
	 * @return false in case of error
	 */
	bool loadCanvas(canvas::SessionLoader &loader);

	/**
	 * @brief Save the canvas content
	 *
	 * Saving sets the current filename and marks the document as not dirty
	 *
	 * @param filename the file to save to
	 * @return true on success
	 */
	bool saveCanvas(const QString &filename);

	void setAutosave(bool autosave);
	bool isAutosave() const { return m_autosave; }
	bool canAutosave() const { return m_canAutosave; }

	QString sessionTitle() const;

	QString currentFilename() const { return m_currentFilename; }

	bool isRecording() const { return m_recorder != nullptr; }
	bool startRecording(const QString &filename, QString *error=nullptr);
	void stopRecording();

	bool isDirty() const { return m_dirty; }
	bool isServerSpaceLow() const { return m_serverSpaceLow; }

	bool isSessionClosed() const { return m_sessionClosed; }
	bool isSessionPreserveChat() const { return m_sessionPreserveChat; }
	bool isSessionPasswordProtected() const { return m_sessionPasswordProtected; }

	void setAutoRecordOnConnect(bool autorec) { m_autoRecordOnConnect = autorec; }

signals:
	//! Connection opened, but not yet logged in
	void serverConnected(const QString &address, int port);
	void serverLoggedin();
	void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);

	void canvasChanged(canvas::CanvasModel *canvas);
	void dirtyCanvas(bool isDirty);
	void autosaveChanged(bool autosave);
	void canAutosaveChanged(bool canAutosave);
	void currentFilenameChanged(const QString &filename);
	void recorderStateChanged(bool recording);

	void sessionTitleChanged(const QString &title);
	void sessionPreserveChatChanged(bool pc);
	void sessionClosedChanged(bool closed);
	void sessionPasswordChanged(bool passwordProtected);
	void serverSpaceLowChanged(bool isLow);

public slots:
	// Convenience slots
	void sendPointerMove(const QPointF &point);
	void sendCloseSession(bool close);
	void sendPasswordChange(const QString &password);
	void sendResetSession(const canvas::StateSavepoint &savepoint);
	void sendLockSession(bool lock);
	void sendLockImageCommands(bool lock);
	void sendLayerCtrlMode(bool lockCtrl, bool ownLayers);
	void sendResizeCanvas(int top, int right, int bottom, int left);

	// Tool related functions
	void undo();
	void redo();

	void selectAll(); // Note: selection tool should be activated before calling this
	void selectNone();
	void cancelSelection();

	void copyVisible();
	void copyLayer();
	void cutLayer();
	void pasteImage(const QImage &image, const QPoint &point, bool forcePoint); // Note: selection tool should be activated before calling this
	void stamp();

	void removeEmptyAnnotations();
	void fillArea(const QColor &color, paintcore::BlendMode::Mode mode);

private slots:
	void onServerLogin(bool join);
	void onServerDisconnect();

	void onSessionConfChanged(const QJsonObject &config);
	void onServerHistoryLimitReceived(int maxSpace);

	void snapshotNeeded();
	void markDirty();
	void unmarkDirty();

	void autosaveNow();
private:
	void setCurrentFilename(const QString &filename);
	void setSessionClosed(bool closed);
	void setSessionPreserveChat(bool pc);
	void setSessionPasswordProtected(bool pp);

	void copyFromLayer(int layer);

	void autosave();

	QString m_currentFilename;

	QList<protocol::MessagePtr> m_resetstate;

	canvas::CanvasModel *m_canvas;
	tools::ToolController *m_toolctrl;
	net::Client *m_client;

	recording::Writer *m_recorder;
	bool m_autoRecordOnConnect;

	bool m_dirty;
	bool m_autosave;
	bool m_canAutosave;
	QTimer *m_autosaveTimer;

	bool m_sessionClosed;
	bool m_sessionPreserveChat;
	bool m_sessionPasswordProtected;
	bool m_serverSpaceLow;
};

#endif // DOCUMENT_H

