// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWPILE_DOCUMENT_H
#define DRAWPILE_DOCUMENT_H
extern "C" {
#include <dpengine/load.h>
#include <dpengine/save.h>
#include <dpmsg/acl.h>
#include <dpmsg/blend_mode.h>
}
#include "libclient/drawdance/paintengine.h"
#include "libclient/net/announcementlist.h"
#include "libclient/net/authlistmodel.h"
#include "libclient/net/banlistmodel.h"
#include "libclient/net/message.h"
#include "libshared/util/qtcompat.h"
#include <QObject>
#include <QStringListModel>
#ifdef Q_OS_ANDROID
#	include <QMimeData>
#endif

class QMimeData;
class QString;
class QTemporaryDir;
class QTimer;

namespace canvas {
class CanvasModel;
}
namespace net {
class Client;
class BanlistModel;
class AnnouncementListModel;
}
namespace libclient {
namespace settings {
class Settings;
}
}
namespace tools {
class ToolController;
}

/**
 * @brief An active document and its associated data, including the network
 * connection
 *
 * This is an UI agnostic class that should be usable from both a widget
 * based application or a pure QML app.
 *
 */
class Document final : public QObject {
	Q_PROPERTY(canvas::CanvasModel *canvas READ canvas() NOTIFY canvasChanged)
	Q_PROPERTY(net::BanlistModel *banlist READ banlist() CONSTANT)
	Q_PROPERTY(net::AuthListModel *authList READ authList() CONSTANT)
	Q_PROPERTY(net::AnnouncementListModel *announcementList READ
				   announcementList() CONSTANT)
	Q_PROPERTY(QStringListModel *serverLog READ serverLog() CONSTANT)
	Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyCanvas)
	Q_PROPERTY(
		bool autosave READ isAutosave WRITE setAutosave NOTIFY autosaveChanged)
	Q_PROPERTY(bool canAutosave READ canAutosave NOTIFY canAutosaveChanged)
	Q_PROPERTY(
		QString sessionTitle READ sessionTitle NOTIFY sessionTitleChanged)
	Q_PROPERTY(QString currentPath READ currentPath() NOTIFY currentPathChanged)
	Q_PROPERTY(bool recording READ isRecording() NOTIFY recorderStateChanged)

	Q_PROPERTY(bool sessionPersistent READ isSessionPersistent NOTIFY
				   sessionPersistentChanged)
	Q_PROPERTY(
		bool sessionClosed READ isSessionClosed NOTIFY sessionClosedChanged)
	Q_PROPERTY(bool sessionAuthOnly READ isSessionAuthOnly NOTIFY
				   sessionAuthOnlyChanged)
	Q_PROPERTY(bool sessionPreserveChat READ isSessionPreserveChat NOTIFY
				   sessionPreserveChatChanged)
	Q_PROPERTY(bool sessionPasswordProtected READ isSessionPasswordProtected
				   NOTIFY sessionPasswordChanged)
	Q_PROPERTY(
		bool sessionHasOpword READ isSessionOpword NOTIFY sessionOpwordChanged)
	Q_PROPERTY(bool sessionNsfm READ isSessionNsfm NOTIFY sessionNsfmChanged)
	Q_PROPERTY(bool sessionForceNsfm READ isSessionForceNsfm NOTIFY
				   sessionForceNsfmChanged)
	Q_PROPERTY(bool sessionDeputies READ isSessionDeputies NOTIFY
				   sessionDeputiesChanged)
	Q_PROPERTY(int sessionMaxUserCount READ sessionMaxUserCount NOTIFY
				   sessionMaxUserCountChanged)
	Q_PROPERTY(double sessionResetThreshold READ sessionResetThreshold NOTIFY
				   sessionResetThresholdChanged)
	Q_PROPERTY(double baseResetThreshold READ baseResetThreshold NOTIFY
				   baseResetThresholdChanged)

	Q_OBJECT
public:
	explicit Document(
		int canvasImplementation, libclient::settings::Settings &settings,
		QObject *parent = nullptr);

	canvas::CanvasModel *canvas() const { return m_canvas; }
	tools::ToolController *toolCtrl() const { return m_toolctrl; }
	net::Client *client() const { return m_client; }
	net::BanlistModel *banlist() const { return m_banlist; }
	net::AuthListModel *authList() const { return m_authList; }
	net::AnnouncementListModel *announcementList() const
	{
		return m_announcementlist;
	}
	QStringListModel *serverLog() const { return m_serverLog; }

	/**
	 * @brief (Re)initialize the canvas
	 *
	 * This deletes the old canvas (if it exists) and creates a fresh one.
	 */
	void initCanvas();

	bool loadBlank(const QSize &size, const QColor &background);
	void loadState(
		const drawdance::CanvasState &canvasState, const QString &path,
		DP_SaveImageType type, bool dirty);
	DP_LoadResult loadRecording(
		const QString &path, bool debugDump, bool *outIsTemplate = nullptr);

	void
	saveCanvasAs(const QString &path, DP_SaveImageType type, bool exported);
	void saveCanvasStateAs(
		const QString &path, DP_SaveImageType type,
		const drawdance::CanvasState &canvasState, bool isCurrentState,
		bool exported);
	void exportTemplate(const QString &path);
	bool saveSelection(const QString &path);
	bool isSaveInProgress() const { return m_saveInProgress; }

	void setAutosave(bool autosave);
	bool isAutosave() const { return m_autosave; }
	bool canAutosave() const { return m_canAutosave; }

#ifdef __EMSCRIPTEN__
	void downloadCanvas(
		const QString &fileName, DP_SaveImageType type, QTemporaryDir *tempDir);
	void downloadCanvasState(
		const QString &fileName, DP_SaveImageType type, QTemporaryDir *tempDir,
		const drawdance::CanvasState &canvasState);
	void downloadSelection(const QString &fileName);
#endif

	void setWantCanvasHistoryDump(bool wantCanvasHistoryDump);
	bool wantCanvasHistoryDump() const { return m_wantCanvasHistoryDump; }

	QString sessionTitle() const;

	QString currentPath() const { return m_currentPath; }
	DP_SaveImageType currentType() const { return m_currentType; }

	QString downloadName() const;
	void setDownloadName(const QString &downloadName);

	bool isRecording() const;
	drawdance::RecordStartResult startRecording(const QString &filename);
	bool stopRecording();

	bool isDirty() const { return m_dirty; }

	bool isSessionPersistent() const { return m_sessionPersistent; }
	bool isSessionClosed() const { return m_sessionClosed; }
	bool isSessionAuthOnly() const { return m_sessionAuthOnly; }
	bool isSessionAllowWeb() const { return m_sessionAllowWeb; }
	bool isSessionPreserveChat() const { return m_sessionPreserveChat; }
	bool isSessionPasswordProtected() const
	{
		return m_sessionPasswordProtected;
	}
	bool isSessionOpword() const { return m_sessionOpword; }
	bool isSessionNsfm() const { return m_sessionNsfm; }
	bool isSessionForceNsfm() const { return m_sessionForceNsfm; }
	bool isSessionDeputies() const { return m_sessionDeputies; }
	int sessionMaxUserCount() const { return m_sessionMaxUserCount; }
	double sessionResetThreshold() const
	{
		return m_sessionResetThreshold / double(1024 * 1024);
	}
	double baseResetThreshold() const
	{
		return m_baseResetThreshold / double(1024 * 1024);
	}

	bool isSessionOutOfSpace() const { return m_sessionOutOfSpace; }

	void setRecordOnConnect(const QString &filename)
	{
		m_recordOnConnect = filename;
	}

	qulonglong pasteId() const { return reinterpret_cast<uintptr_t>(this); }

	bool isCompatibilityMode() const;

signals:
	//! Connection opened, but not yet logged in
	void serverConnected(const QString &address, int port);
	void serverLoggedIn(bool join, const QString &joinPassword);
	void serverDisconnected(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);
	void compatibilityModeChanged(bool compatibilityMode);

	void canvasChanged(canvas::CanvasModel *canvas);
	void dirtyCanvas(bool isDirty);
	void autosaveChanged(bool autosave);
	void canAutosaveChanged(bool canAutosave);
	void currentPathChanged(const QString &path);
	void recorderStateChanged(bool recording);

	void sessionTitleChanged(const QString &title);
	void sessionPreserveChatChanged(bool pc);
	void sessionPersistentChanged(bool p);
	void sessionClosedChanged(bool closed);
	void sessionAuthOnlyChanged(bool closed);
	void sessionAllowWebChanged(bool allowWeb, bool canAlter);
	void sessionPasswordChanged(bool passwordProtected);
	void sessionOpwordChanged(bool opword);
	void sessionNsfmChanged(bool nsfm);
	void sessionForceNsfmChanged(bool forceNsfm);
	void sessionDeputiesChanged(bool deputies);
	void sessionIdleChanged(int timeLimit, bool overridden, bool canOverride);
	void sessionMaxUserCountChanged(int count);
	void sessionResetThresholdChanged(double threshold);
	void baseResetThresholdChanged(double threshold);
	void autoResetTooLarge(int maxSize);
	void sessionOutOfSpaceChanged(bool outOfSpace);
	void sessionResetState(const drawdance::CanvasState &canvasState);

	void catchupProgress(int percent);

	void canvasSaveStarted();
	void canvasSaved(const QString &errorMessage);
	void canvasDownloadStarted();
	void
	canvasDownloadReady(const QString &defaultName, const QByteArray &bytes);
	void canvasDownloadError(const QString &error);
	void templateExported(const QString &errorMessage);

	void justInTimeSnapshotGenerated();

public slots:
	// Convenience slots
	void sendPointerMove(const QPointF &point);
	void sendSessionConf(const QJsonObject &sessionconf);
	void sendFeatureAccessLevelChange(const uint8_t[DP_FEATURE_COUNT]);
	void sendLockSession(bool lock = true);
	void sendOpword(const QString &opword);
	void sendResetSession(const net::MessageList &resetImage = {});
	void sendResizeCanvas(int top, int right, int bottom, int left);
	void sendUnban(int entryId);
	void sendAnnounce(const QString &url);
	void sendUnannounce(const QString &url);
	void sendTerminateSession(const QString &reason);
	void sendCanvasBackground(const QColor &color);
	void sendAbuseReport(int userId, const QString &message);

	// Tool related functions
	void undo();
	void redo();

	void
	selectAll(); // Note: selection tool should be activated before calling this
	void selectNone();
	void cancelSelection();

	void copyVisible();
	void copyMerged();
	void copyLayer();
	void cutLayer();
	void pasteImage(
		const QImage &image, const QPoint &point,
		bool forcePoint); // Note: selection tool should be activated before
						  // calling this
	void stamp();

	void removeEmptyAnnotations();
	void clearArea();
	void fillArea(const QColor &color, DP_BlendMode mode, bool source = false);

	void addServerLogEntry(const QString &log);

	static const QMimeData *getClipboardData();

private slots:
	void onServerLogin(
		bool join, bool compatibilityMode, const QString &joinPassword,
		const QString &authId);
	void onServerDisconnect();
	void onSessionResetted();
	void onSessionOutOfSpace();

	void onSessionConfChanged(const QJsonObject &config);
	void onAutoresetRequested(int maxSize, bool query);
	void onMoveLayerRequested(
		int sourceId, int targetId, bool intoGroup, bool below);

	void snapshotNeeded();
	void markDirty();
	void unmarkDirty();

	void autosaveNow();
	void onCanvasSaved(const QString &errorMessage);

private:
	void saveCanvasState(
		const drawdance::CanvasState &canvasState, bool isCurrentState,
		const QString &path, DP_SaveImageType type);
	QImage selectionToImage();
	void setCurrentPath(const QString &path, DP_SaveImageType type);
	void setSessionPersistent(bool p);
	void setSessionClosed(bool closed);
	void setSessionAuthOnly(bool authOnly);
	void setSessionAllowWeb(bool allowWeb);
	void setSessionPreserveChat(bool pc);
	void setSessionPasswordProtected(bool pp);
	void setSessionOpword(bool ow);
	void setSessionMaxUserCount(int count);
	void setSessionResetThreshold(int threshold);
	void setBaseResetThreshold(int threshold);
	void setSessionNsfm(bool nsfm);
	void setSessionForceNsfm(bool forceNsfm);
	void setSessionDeputies(bool deputies);
	void setSessionIdleTimeLimit(int idleTimeLimit);
	void setSessionIdleOverride(bool idleOverride);
	void setSessionAllowIdleOverride(bool allowIdleOverride);
	void setSessionOutOfSpace(bool outOfSpace);

	bool copyFromLayer(int layer);
	void fillBackground(QImage &img);

	void autosave();

	void generateJustInTimeSnapshot();
	void sendResetSnapshot();

	QString m_currentPath;
	DP_SaveImageType m_currentType = DP_SAVE_IMAGE_UNKNOWN;
	QString m_downloadName;

	const int m_canvasImplementation;
	net::MessageList m_resetstate;
	net::MessageList m_messageBuffer;

	canvas::CanvasModel *m_canvas;
	tools::ToolController *m_toolctrl;
	net::Client *m_client;
	net::BanlistModel *m_banlist;
	net::AuthListModel *m_authList;
	net::AnnouncementListModel *m_announcementlist;
	QStringListModel *m_serverLog;
	libclient::settings::Settings &m_settings;

	QString m_originalRecordingFilename;
	QString m_recordOnConnect;

	bool m_dirty;
	bool m_autosave;
	bool m_canAutosave;
	bool m_saveInProgress;
	bool m_wantCanvasHistoryDump;
	QTimer *m_autosaveTimer;

	bool m_sessionPersistent;
	bool m_sessionClosed;
	bool m_sessionAuthOnly;
	bool m_sessionAllowWeb;
	bool m_sessionPreserveChat;
	bool m_sessionPasswordProtected;
	bool m_sessionOpword;
	bool m_sessionNsfm;
	bool m_sessionForceNsfm;
	bool m_sessionDeputies;
	bool m_sessionIdleOverride;
	bool m_sessionAllowIdleOverride;

	int m_sessionMaxUserCount;
	int m_sessionHistoryMaxSize;
	int m_sessionResetThreshold;
	int m_baseResetThreshold;
	int m_sessionIdleTimeLimit;

	bool m_sessionOutOfSpace;

#ifdef HAVE_CLIPBOARD_EMULATION
	static QMimeData clipboardData;
#endif
};

#endif // DOCUMENT_H
