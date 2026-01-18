// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWPILE_DOCUMENT_H
#define DRAWPILE_DOCUMENT_H
extern "C" {
#include <dpengine/load_enums.h>
#include <dpengine/save_enums.h>
#include <dpmsg/acl.h>
#include <dpmsg/blend_mode.h>
}
#include "libclient/drawdance/paintengine.h"
#include "libclient/net/announcementlist.h"
#include "libclient/net/authlistmodel.h"
#include "libclient/net/banlistmodel.h"
#include "libclient/net/client.h"
#include "libclient/net/message.h"
#include <QJsonObject>
#include <QObject>
#include <QQueue>
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
class ReconnectState;
}

namespace config {
class Config;
}

namespace net {
class AnnouncementListModel;
class BanlistModel;
class InviteListModel;
class LoginHandler;
}

namespace tools {
class ToolController;
}

class Document final : public QObject, public net::Client::CommandHandler {
	Q_OBJECT
public:
	enum class StreamResetState { None, Generating, Streaming };

	explicit Document(
		int canvasImplementation, config::Config *cfg,
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
	net::InviteListModel *inviteList() const { return m_inviteList; }
	QStringListModel *serverLog() const { return m_serverLog; }

	/**
	 * @brief (Re)initialize the canvas
	 *
	 * This deletes the old canvas (if it exists) and creates a fresh one.
	 */
	void initCanvas();

	bool loadBlank(
		const QSize &size, const QColor &background,
		const QString &initialLayerName, const QString &initialTrackName,
		bool autoRecord);
	void loadState(
		const drawdance::CanvasState &canvasState, const QString &path,
		DP_SaveImageType type, bool dirty, bool autoRecord);
	DP_LoadResult loadRecording(
		const QString &path, bool debugDump, bool *outIsTemplate = nullptr);

	void saveCanvasAs(
		const QString &path, DP_SaveImageType type, bool exported, bool append);
	void saveCanvasStateAs(
		const QString &path, DP_SaveImageType type,
		const drawdance::CanvasState &canvasState, bool isCurrentState,
		bool exported, bool append);
	void exportTemplate(const QString &path);
	bool saveSelection(const QString &path);
	bool isSaveInProgress() const { return m_saveInProgress; }

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

	bool haveCurrentPath() const { return !m_currentPath.isEmpty(); }
	bool haveExportPath() const { return !m_exportPath.isEmpty(); }
	QString currentPath() const { return m_currentPath; }
	QString exportPath() const { return m_exportPath; }
	DP_SaveImageType currentType() const { return m_currentType; }
	DP_SaveImageType exportType() const { return m_exportType; }
	void clearPaths();

	QString downloadName() const;
	void setDownloadName(const QString &downloadName);

	bool isRecording() const;
	drawdance::RecordStartResult startRecording(const QString &filename);
	bool stopRecording();

	bool isDirty() const;

	bool isSessionPersistent() const { return m_sessionPersistent; }
	bool isSessionClosed() const { return m_sessionClosed; }
	bool isSessionAuthOnly() const { return m_sessionAuthOnly; }
	bool isSessionWebSupported() const { return m_sessionWebSupported; }
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
	bool isSessionPreferWebSockets() const { return m_sessionPreferWebSockets; }
	bool isSessionInviteCodesEnabled() const
	{
		return m_sessionInviteCodesEnabled;
	}
	bool serverSupportsInviteCodes() const
	{
		return m_serverSupportsInviteCodes;
	}
	bool isPreparingReset() const { return m_preparingReset; }

	bool isStreamingReset() const
	{
		return m_streamResetState != StreamResetState::None;
	}

	void setRecordOnConnect(const QString &filename)
	{
		m_recordOnConnect = filename;
	}

	void setProjectRecordOnConnect(bool projectRecordOnConnect)
	{
		m_projectRecordOnConnect = projectRecordOnConnect;
	}

	void connectToServer(
		int timeoutSecs, int proxyMode, int connectStrategy,
		net::LoginHandler *loginhandler, bool builtin);

	qulonglong pasteId() const { return reinterpret_cast<uintptr_t>(this); }

	bool isCompatibilityMode() const;
	bool isMinorIncompatibility() const;

	void handleCommands(int count, const net::Message *msgs) override;
	void handleLocalCommands(int count, const net::Message *msgs) override;

	bool checkPermission(int feature);

	void setReconnectStatePrevious(int layerId, int trackId, int frameIndex);
	void clearReconnectState();

signals:
	//! Connection opened, but not yet logged in
	void serverConnected(const QUrl &url);
	void serverRedirected(const QUrl &url);
	void serverSocketTypeChanged(const QString &socketType);
	void serverLoggedIn(bool join, const QString &joinPassword);
	void serverDisconnected(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);
	void
	serverDisconnectedAgain(const QString &message, const QString &errorcode);
	void compatibilityModeChanged(bool compatibilityMode);

	void canvasChanged(canvas::CanvasModel *canvas);
	void dirtyCanvas(bool isDirty);
	void currentPathChanged(const QString &path);
	void exportPathChanged(const QString &path);
	void recorderStateChanged(bool recording);

	void sessionTitleChanged(const QString &title);
	void sessionPreserveChatChanged(bool pc);
	void sessionPersistentChanged(bool p);
	void sessionClosedChanged(bool closed);
	void sessionAuthOnlyChanged(bool closed);
	void sessionWebSupportedChanged(bool sessionWebSupported);
	void sessionAllowWebChanged(bool allowWeb, bool canAlter);
	void sessionHasPasswordChanged(bool passwordProtected);
	void sessionPasswordChanged(const QString &password);
	void sessionOpwordChanged(bool opword);
	void sessionNsfmChanged(bool nsfm);
	void sessionForceNsfmChanged(bool forceNsfm);
	void sessionDeputiesChanged(bool deputies);
	void sessionIdleChanged(int timeLimit, bool overridden, bool canOverride);
	void sessionMaxUserCountChanged(int count);
	void sessionResetThresholdChanged(double threshold);
	void baseResetThresholdChanged(double threshold);
	void resetImageTooLarge(int maxSize, bool autoReset);
	void sessionOutOfSpaceChanged(bool outOfSpace);
	void sessionPreferWebSocketsChanged(bool preferWebSockets);
	void sessionInviteCodesEnabledChanged(bool inviteCodesEnabled);
	void serverSupportsInviteCodesChanged(bool serverSupportsInviteCodes);
	void preparingResetChanged(bool preparingReset);
	void sessionResetState(const drawdance::CanvasState &canvasState);

	void catchupProgress(int percent);
	void streamResetProgress(int percent);

	void canvasSaveStarted();
	void canvasSaved(const QString &errorMessage, qint64 elapsedMs);
	void canvasDownloadStarted();
	void canvasDownloadReady(
		const QString &defaultName, const QByteArray &bytes,
		qint64 elapsedMsec);
	void canvasDownloadError(const QString &error);
	void templateExported(const QString &errorMessage);

	void justInTimeSnapshotGenerated(bool autoReset);
	void buildStreamResetImageFinished(
		const net::MessageList &image, int messageCount,
		const QString &correlator);

	void permissionDenied(int feature);

public slots:
	// Convenience slots
	void sendPointerMove(const QPointF &point);
	void sendSessionConf(const QJsonObject &sessionconf);
	void sendFeatureAccessLevelChange(const QVector<uint8_t> &tiers);
	void sendFeatureLimitsChange(const QVector<int32_t> &limits);
	void sendLockSession(bool lock = true);
	void sendOpword(const QString &opword);
	void sendResetSession(
		const net::MessageList &resetImage = {},
		const QString &type = QString());
	void sendResizeCanvas(int top, int right, int bottom, int left);
	void sendUnban(int entryId);
	void sendAnnounce(const QString &url);
	void sendUnannounce(const QString &url);
	void sendTerminateSession(const QString &reason);
	void sendCanvasBackground(const QColor &color);
	void sendAbuseReport(int userId, const QString &message);
	void sendCreateInviteCode(int maxUses, bool op, bool trust);
	void sendRemoveInviteCode(const QString &secret);
	void sendInviteCodesEnabled(bool enabled);

	// Tool related functions
	void undo();
	void redo();

	void selectAll();
	void selectNone(bool finishTransform);
	void selectInvert();
	void selectLayerBounds();
	void selectLayerContents();
	void selectMask(const QImage &img, int x, int y);

	void copyVisible();
	void copyMerged();
	void copyLayer();
	void cutLayer();

	void deleteAnnotation(int annotationId);
	void removeEmptyAnnotations();
	void clearArea();
	void fillArea(const QColor &color, DP_BlendMode mode, float opacity);

	void addServerLogEntry(const QString &log);

	static const QMimeData *getClipboardData();
	static bool clipboardHasImageData();
	static QImage getClipboardImageData(const QMimeData *mimeData);

private slots:
	void onServerLogin(const net::LoggedInParams &params);
	void onServerDisconnect(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);
	void setPreparingReset(bool preparing);
	void onSessionResetted();
	void onSessionOutOfSpace();

	void onLagMeasured(qint64 msec);
	void onSessionConfChanged(const QJsonObject &config);
	void onAutoresetQueried(int maxSize, const QString &payload);
	void onAutoresetRequested(
		int maxSize, const QString &correlator, const QString &stream);
	void onStreamResetStarted(const QString &correlator);
	void onStreamResetProgressed(bool cancel);
	void buildStreamResetImage(
		const drawdance::CanvasState &canvasState, const QString &correlator);
	void onMoveLayerRequested(
		const QVector<int> &sourceIds, int targetId, bool intoGroup,
		bool below);

	void snapshotNeeded();
	void markDirty();
	void unmarkDirty();

	void onSaveSucceeded(qint64 elapsedMsec);
	void onSaveCancelled();
	void onSaveFailed(const QString &errorMessage);

private:
	void clearConfig();

	void saveCanvasState(
		const drawdance::CanvasState &canvasState, bool isCurrentState,
		bool exported, bool append, const QString &path, DP_SaveImageType type);
	QImage selectionToImage();
	void setCurrentPath(const QString &path, DP_SaveImageType type);
	void setExportPath(const QString &path, DP_SaveImageType type);
	void setSessionPersistent(bool p);
	void setSessionClosed(bool closed);
	void setSessionAuthOnly(bool authOnly);
	void setSessionWebSupported(bool sessionWebSupported);
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
	void setSessionPreferWebSockets(bool sessionPreferWebSockets);
	void setSessionInviteCodesEnabled(bool sessionInviteCodesEnabled);
	void setServerSupportsInviteCodes(bool serverSupportsInviteCodes);

	bool copyFromLayer(int layer);
	void fillBackground(QImage &img);
	void selectLayer(bool includeMask);
	void selectAllOp(int op);
	void selectOp(int op, const QRect &bounds, const QImage &mask = QImage());

	bool shouldRespondToAutoReset() const;
	void generateJustInTimeSnapshot();
	void sendResetSnapshot(bool autoReset);
	bool isResetStateSizeInLimit() const;
	unsigned long long resetStateSize() const;

	net::MessageList generateStreamSnapshot(
		const drawdance::CanvasState &canvasState,
		const net::MessageList &metadata, int prepended,
		int &outMessageCount) const;

	void startSendingStreamResetSnapshot(
		const net::MessageList &image, int messageCount,
		const QString &correlator);
	void sendNextStreamResetMessage();
	void setStreamResetState(StreamResetState state, int messageCount = 0);
	void emitStreamResetProgress();

	void onThumbnailQueried(const QString &payload);
	void onThumbnailRequested(
		const QByteArray &correlator, int maxWidth, int maxHeight, int quality,
		const QString &format);
	void onThumbnailGenerationFinished(const net::Message &msg);
	void onThumbnailGenerationFailed(
		const QByteArray &correlator, const QString &error);

	QString m_currentPath;
	QString m_exportPath;
	DP_SaveImageType m_currentType = DP_SAVE_IMAGE_UNKNOWN;
	DP_SaveImageType m_exportType = DP_SAVE_IMAGE_UNKNOWN;
	QString m_downloadName;

	const int m_canvasImplementation;
	net::MessageList m_resetstate;
	net::MessageList m_streamResetImage;
	net::MessageList m_messageBuffer;
	QQueue<qint64> m_pingHistory;

	canvas::CanvasModel *m_canvas = nullptr;
	tools::ToolController *m_toolctrl;
	net::Client *m_client;
	net::BanlistModel *m_banlist;
	net::AuthListModel *m_authList;
	net::AnnouncementListModel *m_announcementlist;
	net::InviteListModel *m_inviteList;
	canvas::ReconnectState *m_reconnectState = nullptr;
	QStringListModel *m_serverLog;
	config::Config *m_cfg;

	QString m_originalRecordingFilename;
	QString m_recordOnConnect;
	QString m_autoResetCorrelator;
	bool m_projectRecordOnConnect = false;

	bool m_saveInProgress = false;
	bool m_wantCanvasHistoryDump = false;
	bool m_generatingThumbnail = false;

	QJsonObject m_cumulativeConfig;
	bool m_sessionPersistent = false;
	bool m_sessionClosed = false;
	bool m_sessionAuthOnly = false;
	bool m_sessionWebSupported = false;
	bool m_sessionAllowWeb = false;
	bool m_sessionPreserveChat = false;
	bool m_sessionPasswordProtected = false;
	bool m_sessionOpword = false;
	bool m_sessionNsfm = false;
	bool m_sessionForceNsfm = false;
	bool m_sessionDeputies = false;
	bool m_sessionIdleOverride = false;
	bool m_sessionAllowIdleOverride = false;
	bool m_sessionPreferWebSockets = false;
	bool m_sessionInviteCodesEnabled = false;
	int m_sessionMaxUserCount = 0;
	int m_sessionHistoryMaxSize = 0;
	int m_sessionResetThreshold = 0;
	int m_baseResetThreshold = 0;
	int m_sessionIdleTimeLimit = 0;
	bool m_serverSupportsInviteCodes = false;
	int m_streamResetMessageCount = 0;
	int m_streamResetImageOriginalCount = 0;
	StreamResetState m_streamResetState = StreamResetState::None;

	bool m_sessionOutOfSpace = false;
	bool m_preparingReset = false;

#ifdef HAVE_CLIPBOARD_EMULATION
	static QMimeData clipboardData;
#endif
};

#endif // DOCUMENT_H
