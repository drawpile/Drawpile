// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IMPORT_CANVASLOADERRUNNABLE
#define LIBCLIENT_IMPORT_CANVASLOADERRUNNABLE
extern "C" {
#include <dpengine/load_enums.h>
}
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/viewstate.h"
#include <QObject>
#include <QRunnable>
#include <QString>

class CanvasLoaderRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	explicit CanvasLoaderRunnable(
		const QString &path, bool guessPlayer, QObject *parent = nullptr);

	void run() override;

	const QString &path() const { return m_path; }
	DP_LoadResult result() const { return m_result; }
	DP_SaveImageType type() const { return m_type; }
	const drawdance::CanvasState &canvasState() const { return m_canvasState; }
	const drawdance::ViewState &viewState() const { return m_viewState; }
	const QString &sessionSourceParam() const { return m_sessionSourceParam; }
	long long sessionSequenceId() const { return m_sessionSequenceId; }
	long long resumeSessionId() const { return m_resumeSessionId; }
	unsigned int playerFlags() const { return m_playerFlags; }
	bool looksLikeRecording() const { return m_playerFlags != 0u; }

signals:
	void loadComplete(
		const QString &error, const QString &detail, qint64 elapsedMsec);

private:
	const QString m_path;
	const bool m_guessPlayer;
	DP_LoadResult m_result = DP_LOAD_RESULT_BAD_ARGUMENTS;
	DP_SaveImageType m_type = DP_SAVE_IMAGE_UNKNOWN;
	drawdance::CanvasState m_canvasState;
	drawdance::ViewState m_viewState;
	QString m_sessionSourceParam;
	long long m_sessionSequenceId = -1LL;
	long long m_resumeSessionId = 0LL;
	unsigned int m_playerFlags = 0u;
};

#endif
