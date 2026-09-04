// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpimpex/image_impex.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/project.h"
#include "libshared/net/message.h"
#include <QString>
#include <QUuid>

namespace drawdance {

Project::~Project()
{
	if(m_data) {
		closeWarn();
	}
}

bool Project::open(
	const QString &path, unsigned int flags, int *outError, int *outSqlResult)
{
	DP_ProjectOpenResult result =
		DP_project_open(path.toUtf8().constData(), flags);
	if(outError) {
		*outError = result.error;
	}
	if(outSqlResult) {
		*outSqlResult = result.sql_result;
	}

	if(result.error == 0) {
		if(m_data) {
			closeWarn();
		}
		m_data = result.project;
		return true;
	} else {
		return false;
	}
}

bool Project::close()
{
	bool ok = DP_project_close(m_data);
	m_data = nullptr;
	return ok;
}

int Project::openNewSession(int sourceType, unsigned int flags)
{
	return openSession(
		sourceType, generateSourceParam().constData(), DP_PROTOCOL_VERSION,
		flags);
}

int Project::setSessionThumbnail(
	const CanvasState &canvasState, DP_DrawContext *dc)
{
	return DP_project_session_thumbnail_set(
		m_data, canvasState.get(), dc, DP_image_write_project_thumbnail,
		nullptr);
}

int Project::saveSession(const CanvasState &canvasState)
{
	return DP_project_session_save(
		m_data, canvasState.get(), DP_image_write_project_thumbnail, nullptr);
}

int Project::saveSessionAt(
	long long sessionId, long long sequenceId, const CanvasState &canvasState)
{
	return DP_project_session_save_at(
		m_data, sessionId, sequenceId, canvasState.get(),
		DP_image_write_project_thumbnail, nullptr);
}

int Project::copySession(
	const QString &path, long long sessionId, const CopyCallbackFn &callback)
{
	return DP_project_session_copy(
		m_data, path.toUtf8().constData(), generateSourceParam().constData(),
		sessionId, &Project::copyCallback,
		const_cast<CopyCallbackFn *>(&callback));
}

int Project::info(unsigned int flags, const InfoFn &fn)
{
	return DP_project_info(
		m_data, flags, &Project::infoCallback, const_cast<InfoFn *>(&fn));
}

DP_ProjectCheckResult Project::checkPath(const QString &path)
{
	return DP_project_check_path(path.toUtf8().constData());
}

QByteArray Project::generateSourceParam()
{
	return QUuid::createUuid().toByteArray(QUuid::Id128);
}

void Project::closeWarn()
{
	if(!DP_project_close(m_data)) {
		qWarning("Error closing project: %s", DP_error());
	}
}

int Project::copyCallback(
	void *user, const DP_ProjectCopyCallbackParams *params)
{
	return (*static_cast<const CopyCallbackFn *>(user))(*params);
}

void Project::infoCallback(void *user, const DP_ProjectInfo *info)
{
	(*static_cast<const InfoFn *>(user))(*info);
}

ProjectMessageCompressor::~ProjectMessageCompressor()
{
	DP_project_message_compressor_free(m_data);
}

bool ProjectMessageCompressor::open(Project &project)
{
	DP_ProjectMessageCompressor *pmc =
		DP_project_message_compressor_new(project.get());
	if(pmc) {
		DP_project_message_compressor_free(m_data);
		m_data = pmc;
		return true;
	} else {
		return false;
	}
}

void ProjectMessageCompressor::close()
{
	DP_project_message_compressor_free(m_data);
	m_data = nullptr;
}

int ProjectMessageCompressor::recordMessage(
	double recordedAt, const net::Message &msg, unsigned int flags)
{
	return DP_project_message_compressor_message_record(
		m_data, recordedAt, msg.get(), flags);
}


ProjectPlayer::~ProjectPlayer()
{
	if(m_data) {
		DP_DrawContext *dc = DP_project_player_draw_context(m_data);
		dispose();
		drawdance::DrawContextPool::releaseRaw(dc);
	}
}

bool ProjectPlayer::open(Project &project)
{
	if(m_data) {
		DP_DrawContext *dc = DP_project_player_draw_context(m_data);
		DP_ProjectPlayer *pp = DP_project_player_new(project.get(), dc);
		if(pp) {
			dispose();
			m_data = pp;
			return true;
		} else {
			return false;
		}
	} else {
		DP_DrawContext *dc = drawdance::DrawContextPool::acquireRaw();
		DP_ProjectPlayer *pp = DP_project_player_new(project.get(), dc);
		if(pp) {
			m_data = pp;
			return true;
		} else {
			drawdance::DrawContextPool::releaseRaw(dc);
			return false;
		}
	}
}

int ProjectPlayer::close()
{
	if(m_data) {
		DP_DrawContext *dc = DP_project_player_draw_context(m_data);
		int result = DP_project_player_free(m_data);
		m_data = nullptr;
		drawdance::DrawContextPool::releaseRaw(dc);
		return result;
	} else {
		return 0;
	}
}

int ProjectPlayer::controlFastForward(
	unsigned int controlId, const ControlCallbackFn &fn)
{
	return control(
		DP_PROJECT_PLAYER_CONTROL_FAST_FORWARD, controlId, fn,
		[](DP_ProjectPlayerControlParams &) {
			// Nothing.
		});
}

int ProjectPlayer::controlSeekIds(
	unsigned int controlId, long long sessionId, long long sequenceId,
	const ControlCallbackFn &fn)
{
	return control(
		DP_PROJECT_PLAYER_CONTROL_SEEK_IDS, controlId, fn,
		[sessionId, sequenceId](DP_ProjectPlayerControlParams &params) {
			params.data.seek_ids.session_id = sessionId;
			params.data.seek_ids.sequence_id = sequenceId;
		});
}

void ProjectPlayer::dispose()
{
	int result = DP_project_player_free(m_data);
	if(result != 0) {
		qWarning("Error %d freeing project player: %s", result, DP_error());
	}
}

int ProjectPlayer::control(
	DP_ProjectPlayerControlType type, unsigned int controlId,
	const ControlCallbackFn &fn,
	const std::function<void(DP_ProjectPlayerControlParams &)> &block)
{
	DP_ProjectPlayerControlParams params;
	params.type = type;
	params.control_id = controlId;
	params.fn = &ProjectPlayer::controlCallback;
	params.user = const_cast<ControlCallbackFn *>(&fn);
	block(params);
	return DP_project_player_control(m_data, &params);
}

int ProjectPlayer::controlCallback(
	void *user, DP_ProjectPlayer *pp,
	const DP_ProjectPlayerControlParams *params, int type)
{
	return (*static_cast<const ControlCallbackFn *>(user))(pp, *params, type);
}

}
