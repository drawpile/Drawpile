// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/input.h>
#include <dpcommon/timing.h>
}
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/playback.h"
#include "libclient/drawdance/player.h"
#include "libclient/import/recordingconverter.h"
#include "libclient/io/files.h"
#include "libclient/io/pathinfo.h"
#include "libclient/net/message.h"
#include <QSet>
#include <algorithm>
#include <memory>

namespace impex {

RecordingConverter::RecordingConverter(
	const QString &recordingPath,
	const QSharedPointer<io::TempFile> &outputFile, QObject *parent)
	: QObject(parent)
	, m_inputs({{recordingPath, 0LL, false}})
	, m_tempFile(outputFile)
	, m_saveSnapshots(false)
{
}

RecordingConverter::RecordingConverter(
	const QVector<Input> &inputs,
	const QSharedPointer<io::TempFile> &outputFile, QObject *parent)
	: QObject(parent)
	, m_inputs(inputs)
	, m_tempFile(outputFile)
	, m_saveSnapshots(true)
{
}

void RecordingConverter::run()
{
	runConversion();
	for(InputProject *ip : m_inputProjects) {
		delete ip;
	}
}

void RecordingConverter::cancel()
{
	m_cancelled.storeRelaxed(1);
}

void RecordingConverter::InputProject::sortContinuedSessions()
{
	std::sort(
		continuedSessions.begin(), continuedSessions.end(),
		[](const ContinuedSession &a, const ContinuedSession &b) {
			long long ai = a.continueSessionId;
			long long bi = b.continueSessionId;
			if(ai < bi) {
				return true;
			} else if(ai == bi) {
				return a.continueSequenceId < b.continueSequenceId;
			} else {
				return false;
			}
		});
}

void RecordingConverter::initProgressInputCount()
{
	QSet<QString> paths;
	for(const Input &input : m_inputs) {
		if(input.project) {
			paths.insert(input.path);
		}
	}
	m_progressInputCount = m_inputs.size() + paths.size();
	if(m_inputs.constLast().project) {
		++m_progressInputCount;
	}
}

void RecordingConverter::runConversion()
{
	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	int inputCount = m_inputs.size();
	if(inputCount == 0) {
		Q_EMIT conversionFailed(QStringLiteral("No input files given."));
		return;
	}

	QString outputPath = m_tempFile->path();
	drawdance::Project project;
	int openError;
	if(!project.open(outputPath, DP_PROJECT_OPEN_TRUNCATE, &openError)) {
		Q_EMIT conversionFailed(
			QStringLiteral("Error %1 opening project file %2.")
				.arg(QString::number(openError), outputPath),
			QString::fromUtf8(DP_error()));
		return;
	}

	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	drawdance::ProjectMessageCompressor projectMessageCompressor;
	if(!projectMessageCompressor.open(project)) {
		Q_EMIT conversionFailed(
			QStringLiteral("Error initializing compressor."),
			QString::fromUtf8(DP_error()));
		return;
	}

	// The timing information is all wrong, but the dprec format doesn't contain
	// much in the way of that. We just start at the current time and add
	// guessed timing and intervals on top of that.
	long long timeMsec = DP_time_unix_msec();
	initProgressInputCount();

	for(int i = 0; i < inputCount; ++i) {
		QString errorMessage, detail;
		bool conversionOk = convertInput(
			project, projectMessageCompressor, i, timeMsec, errorMessage,
			detail);

		if(isCancelled()) {
			Q_EMIT conversionCancelled();
			return;
		}

		if(!conversionOk) {
			Q_EMIT conversionFailed(errorMessage, detail);
			return;
		}

		emitConversionProgress(i, 1.0);
	}

	projectMessageCompressor.close();

	int i = 0;
	for(QHash<QString, InputProject *>::key_value_iterator
			it = m_inputProjects.keyValueBegin(),
			end = m_inputProjects.keyValueEnd();
		it != end; ++it) {

		QString errorMessage, detail;
		bool fixupOk = fixupProject(
			project, it->first, it->second, i, errorMessage, detail);

		if(isCancelled()) {
			Q_EMIT conversionCancelled();
			return;
		}

		if(!fixupOk) {
			Q_EMIT conversionFailed(errorMessage, detail);
			return;
		}

		emitFixupProgress(i, 1.0);
		++i;
	}

	if(m_needsFinalReplay) {
		QString errorMessage, detail;
		bool finalSnapshotOk =
			makeFinalSnapshot(project, i, errorMessage, detail);

		if(isCancelled()) {
			Q_EMIT conversionCancelled();
			return;
		}

		if(!finalSnapshotOk) {
			Q_EMIT conversionFailed(errorMessage, detail);
			return;
		}
	}

	Q_EMIT conversionProgressMessage(QStringLiteral("Finishing project…"));
	int fixOrphanedContinuationsResult =
		project.copySessionFixOrphanedContinuations();

	if(isCancelled()) {
		Q_EMIT conversionCancelled();
		return;
	}

	if(fixOrphanedContinuationsResult != 0) {
		Q_EMIT conversionFailed(
			QStringLiteral("Error finishing project."),
			QString::fromUtf8(DP_error()));
		return;
	}

	if(m_lastPercent != 100) {
		Q_EMIT conversionProgress(100);
	}

	if(!project.close()) {
		Q_EMIT conversionFailed(
			QStringLiteral("Error closing project."),
			QString::fromUtf8(DP_error()));
		return;
	}

	Q_EMIT conversionSucceeded();
}

bool RecordingConverter::convertInput(
	drawdance::Project &project,
	drawdance::ProjectMessageCompressor &projectMessageCompressor, int i,
	long long &timeMsec, QString &outErrorMessage, QString &outDetail)
{
	if(m_inputs[i].project) {
		return convertProjectSession(project, i, outErrorMessage, outDetail);
	} else {
		return convertRecording(
			project, projectMessageCompressor, i, timeMsec, outErrorMessage,
			outDetail);
	}
}

bool RecordingConverter::convertProjectSession(
	drawdance::Project &project, int i, QString &outErrorMessage,
	QString &outDetail)
{
	const Input &input = m_inputs[i];
	QString basename = getBasename(input.path);
	long long sessionId = input.sessionId;
	Q_EMIT conversionProgressMessage(
		QStringLiteral("Processing project %1 session %2…")
			.arg(basename, QString::number(sessionId)));

	InputProject *ip = getInputProject(i, outErrorMessage, outDetail);
	if(!ip) {
		return false;
	}

	bool isLastSession = i == m_inputs.size() - 1;
	int copySessionResult = project.copySession(
		ip->tempFile.fileName(), sessionId,
		[&](const DP_ProjectCopyCallbackParams &params) {
			if(isCancelled()) {
				return 1;
			}

			switch(params.type) {
			case DP_PROJECT_COPY_CALLBACK_SESSION: {
				const DP_ProjectCopyCallbackParamsSession &p =
					params.data.session;
				ip->sessionIdMapping.insert(
					p.source_session_id, p.target_session_id);
				break;
			}
			case DP_PROJECT_COPY_CALLBACK_FILTER_SNAPSHOT: {
				const DP_ProjectCopyCallbackParamsFilterSnapshot &p =
					params.data.filter_snapshot;
				// We want to retain persistent snapshots as well as the final
				// snapshot in the file that allows us to load it quickly if
				// this is the last session in the file. If it's not, we have to
				// create a new snapshot at the end.
				bool shouldInclude =
					(p.flags & DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT) ||
					(isLastSession && p.index == p.count - 1);
				if(shouldInclude) {
					return 0;
				} else {
					return 1;
				}
			}
			case DP_PROJECT_COPY_CALLBACK_SNAPSHOT: {
				const DP_ProjectCopyCallbackParamsSnapshot &p =
					params.data.snapshot;
				ip->sessionIdMapping.insert(
					p.source_snapshot_id, p.target_snapshot_id);
				// Continued snapshots need their ids fixed up or their snapshot
				// reified if the session they continue isn't before them.
				if(p.flags & DP_PROJECT_SNAPSHOT_FLAG_CONTINUATION) {
					ip->continuedSessions.append(
						{p.target_snapshot_id, p.sequence_id,
						 p.continue_session_id, p.continue_sequence_id,
						 !ip->sessionIdMapping.contains(
							 p.continue_session_id)});
				}
				break;
			}
			default:
				break;
			}

			return 0;
		});

	if(isCancelled()) {
		return false;
	}

	if(copySessionResult != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 copying session %2 from project %3.")
				.arg(
					QString::number(copySessionResult),
					QString::number(sessionId), basename);
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	return true;
}

RecordingConverter::InputProject *RecordingConverter::getInputProject(
	int i, QString &outErrorMessage, QString &outDetail)
{
	InputProject *ip = m_inputProjects.value(m_inputs[i].path);
	if(ip) {
		return ip;
	} else {
		return openInputProject(i, outErrorMessage, outDetail);
	}
}

RecordingConverter::InputProject *RecordingConverter::openInputProject(
	int i, QString &outErrorMessage, QString &outDetail)
{
	QString path = m_inputs[i].path;
	QFile inputFile(path);
	if(!inputFile.open(QIODevice::ReadOnly)) {
		outErrorMessage = QStringLiteral("Error %1 opening project %2: %3.")
							  .arg(
								  QString::number(inputFile.error()), path,
								  inputFile.errorString());
		return nullptr;
	}

	std::unique_ptr<InputProject> ipu = std::make_unique<InputProject>();

	QTemporaryFile &tempFile = ipu->tempFile;
	if(!tempFile.open()) {
		outErrorMessage =
			QStringLiteral("Error %1 opening temporary file for %2: %3.")
				.arg(
					QString::number(tempFile.error()), path,
					tempFile.errorString());
		return nullptr;
	}

	if(!io::copyFileContents(inputFile, tempFile, outDetail)) {
		outErrorMessage = QStringLiteral("Error loading project %1.").arg(path);
		return nullptr;
	}

	tempFile.close();

	InputProject *ip = ipu.release();
	m_inputProjects.insert(path, ip);
	return ip;
}

namespace {
struct SetClassicDabsParams {
	const DP_ClassicDab *originalDabs;
	int offset;
};

static void setClassicDabs(int count, DP_ClassicDab *cds, void *user)
{
	SetClassicDabsParams *params = static_cast<SetClassicDabsParams *>(user);
	int offset = params->offset;
	for(int i = 0; i < count; ++i) {
		const DP_ClassicDab *originalDab =
			DP_classic_dab_at(params->originalDabs, i + offset);

		// The first dab of the second message (which has an offset, the first
		// one doesn't) starts at x and y of 0 again.
		int8_t x, y;
		if(offset == 0 || i != 0) {
			x = DP_classic_dab_x(originalDab);
			y = DP_classic_dab_y(originalDab);
		} else {
			x = 0;
			y = 0;
		}

		DP_classic_dab_init(
			cds, i, x, y, DP_classic_dab_size(originalDab),
			DP_classic_dab_hardness(originalDab),
			DP_classic_dab_opacity(originalDab));
	}
}

static void
splitDrawDabsClassic(const net::Message &msg, QVector<net::Message> &outMsgs)
{
	DP_MsgDrawDabsClassic *mddc = msg.toDrawDabsClassic();
	int originalDabCount;
	const DP_ClassicDab *originalDabs =
		DP_msg_draw_dabs_classic_dabs(mddc, &originalDabCount);
	if(originalDabCount > 2) {
		SetClassicDabsParams params = {originalDabs, 0};
		int dabCount1 = originalDabCount / 2;
		outMsgs.append(
			net::Message::noinc(DP_msg_draw_dabs_classic_new(
				msg.contextId(), DP_msg_draw_dabs_classic_flags(mddc),
				DP_msg_draw_dabs_classic_layer(mddc),
				DP_msg_draw_dabs_classic_x(mddc),
				DP_msg_draw_dabs_classic_y(mddc),
				DP_msg_draw_dabs_classic_color(mddc),
				DP_msg_draw_dabs_classic_mode(mddc), setClassicDabs, dabCount1,
				&params)));

		params.offset = dabCount1;
		int32_t x2 = DP_msg_draw_dabs_classic_x(mddc);
		int32_t y2 = DP_msg_draw_dabs_classic_y(mddc);
		for(int i = 0; i < dabCount1; ++i) {
			const DP_ClassicDab *originalDab =
				DP_classic_dab_at(originalDabs, i);
			x2 += int32_t(DP_classic_dab_x(originalDab));
			y2 += int32_t(DP_classic_dab_y(originalDab));
		}

		outMsgs.append(
			net::Message::noinc(DP_msg_draw_dabs_classic_new(
				msg.contextId(), DP_msg_draw_dabs_classic_flags(mddc),
				DP_msg_draw_dabs_classic_layer(mddc), x2, y2,
				DP_msg_draw_dabs_classic_color(mddc),
				DP_msg_draw_dabs_classic_mode(mddc), setClassicDabs,
				originalDabCount - dabCount1, &params)));

	} else {
		// Really shouldn't happen, but handle it anyway.
		outMsgs.append(msg);
	}
}


struct SetPixelDabsParams {
	const DP_PixelDab *originalDabs;
	int offset;
};

static void setPixelDabs(int count, DP_PixelDab *pds, void *user)
{
	SetPixelDabsParams *params = static_cast<SetPixelDabsParams *>(user);
	int offset = params->offset;
	for(int i = 0; i < count; ++i) {
		const DP_PixelDab *originalDab =
			DP_pixel_dab_at(params->originalDabs, i + offset);

		// The first dab of the second message (which has an offset, the first
		// one doesn't) starts at x and y of 0 again.
		int8_t x, y;
		if(offset == 0 || i != 0) {
			x = DP_pixel_dab_x(originalDab);
			y = DP_pixel_dab_y(originalDab);
		} else {
			x = 0;
			y = 0;
		}

		DP_pixel_dab_init(
			pds, i, x, y, DP_pixel_dab_size(originalDab),
			DP_pixel_dab_opacity(originalDab));
	}
}

static void
splitDrawDabsPixel(const net::Message &msg, QVector<net::Message> &outMsgs)
{
	bool square = msg.type() == DP_MSG_DRAW_DABS_PIXEL_SQUARE;
	DP_MsgDrawDabsPixel *mddp = msg.toDrawDabsPixel();
	int originalDabCount;
	const DP_PixelDab *originalDabs =
		DP_msg_draw_dabs_pixel_dabs(mddp, &originalDabCount);
	if(originalDabCount > 2) {
		SetPixelDabsParams params = {originalDabs, 0};
		int dabCount1 = originalDabCount / 2;
		outMsgs.append(
			net::Message::noinc((
				square ? DP_msg_draw_dabs_pixel_square_new
					   : DP_msg_draw_dabs_pixel_new)(
				msg.contextId(), DP_msg_draw_dabs_pixel_flags(mddp),
				DP_msg_draw_dabs_pixel_layer(mddp),
				DP_msg_draw_dabs_pixel_x(mddp), DP_msg_draw_dabs_pixel_y(mddp),
				DP_msg_draw_dabs_pixel_color(mddp),
				DP_msg_draw_dabs_pixel_mode(mddp), setPixelDabs, dabCount1,
				&params)));

		params.offset = dabCount1;
		int32_t x2 = DP_msg_draw_dabs_pixel_x(mddp);
		int32_t y2 = DP_msg_draw_dabs_pixel_y(mddp);
		for(int i = 0; i < dabCount1; ++i) {
			const DP_PixelDab *originalDab = DP_pixel_dab_at(originalDabs, i);
			x2 += int32_t(DP_pixel_dab_x(originalDab));
			y2 += int32_t(DP_pixel_dab_y(originalDab));
		}

		outMsgs.append(
			net::Message::noinc(
				(square ? DP_msg_draw_dabs_pixel_square_new
						: DP_msg_draw_dabs_pixel_new)(
					msg.contextId(), DP_msg_draw_dabs_pixel_flags(mddp),
					DP_msg_draw_dabs_pixel_layer(mddp), x2, y2,
					DP_msg_draw_dabs_pixel_color(mddp),
					DP_msg_draw_dabs_pixel_mode(mddp), setPixelDabs,
					originalDabCount - dabCount1, &params)));

	} else {
		// Really shouldn't happen, but handle it anyway.
		outMsgs.append(msg);
	}
}


struct SetMyPaintDabsParams {
	const DP_MyPaintDab *originalDabs;
	int offset;
};

static void setMyPaintDabs(int count, DP_MyPaintDab *mpds, void *user)
{
	SetMyPaintDabsParams *params = static_cast<SetMyPaintDabsParams *>(user);
	int offset = params->offset;
	for(int i = 0; i < count; ++i) {
		const DP_MyPaintDab *originalDab =
			DP_mypaint_dab_at(params->originalDabs, i + offset);

		// The first dab of the second message (which has an offset, the first
		// one doesn't) starts at x and y of 0 again.
		int8_t x, y;
		if(offset == 0 || i != 0) {
			x = DP_mypaint_dab_x(originalDab);
			y = DP_mypaint_dab_y(originalDab);
		} else {
			x = 0;
			y = 0;
		}

		DP_mypaint_dab_init(
			mpds, i, x, y, DP_mypaint_dab_size(originalDab),
			DP_mypaint_dab_hardness(originalDab),
			DP_mypaint_dab_opacity(originalDab),
			DP_mypaint_dab_angle(originalDab),
			DP_mypaint_dab_aspect_ratio(originalDab));
	}
}

static void
splitDrawDabsMyPaint(const net::Message &msg, QVector<net::Message> &outMsgs)
{
	DP_MsgDrawDabsMyPaint *mddmp = msg.toDrawDabsMyPaint();
	int originalDabCount;
	const DP_MyPaintDab *originalDabs =
		DP_msg_draw_dabs_mypaint_dabs(mddmp, &originalDabCount);
	if(originalDabCount > 2) {
		SetMyPaintDabsParams params = {originalDabs, 0};
		int dabCount1 = originalDabCount / 2;
		outMsgs.append(
			net::Message::noinc(DP_msg_draw_dabs_mypaint_new(
				msg.contextId(), DP_msg_draw_dabs_mypaint_flags(mddmp),
				DP_msg_draw_dabs_mypaint_layer(mddmp),
				DP_msg_draw_dabs_mypaint_x(mddmp),
				DP_msg_draw_dabs_mypaint_y(mddmp),
				DP_msg_draw_dabs_mypaint_color(mddmp),
				DP_msg_draw_dabs_mypaint_lock_alpha(mddmp),
				DP_msg_draw_dabs_mypaint_colorize(mddmp),
				DP_msg_draw_dabs_mypaint_posterize(mddmp),
				DP_msg_draw_dabs_mypaint_mode(mddmp), setMyPaintDabs, dabCount1,
				&params)));

		params.offset = dabCount1;
		int32_t x2 = DP_msg_draw_dabs_mypaint_x(mddmp);
		int32_t y2 = DP_msg_draw_dabs_mypaint_y(mddmp);
		for(int i = 0; i < dabCount1; ++i) {
			const DP_MyPaintDab *originalDab =
				DP_mypaint_dab_at(originalDabs, i);
			x2 += int32_t(DP_mypaint_dab_x(originalDab));
			y2 += int32_t(DP_mypaint_dab_y(originalDab));
		}

		outMsgs.append(
			net::Message::noinc(DP_msg_draw_dabs_mypaint_new(
				msg.contextId(), DP_msg_draw_dabs_mypaint_flags(mddmp),
				DP_msg_draw_dabs_mypaint_layer(mddmp), x2, y2,
				DP_msg_draw_dabs_mypaint_color(mddmp),
				DP_msg_draw_dabs_mypaint_lock_alpha(mddmp),
				DP_msg_draw_dabs_mypaint_colorize(mddmp),
				DP_msg_draw_dabs_mypaint_posterize(mddmp),
				DP_msg_draw_dabs_mypaint_mode(mddmp), setMyPaintDabs,
				originalDabCount - dabCount1, &params)));

	} else {
		// Really shouldn't happen, but handle it anyway.
		outMsgs.append(msg);
	}
}
}

bool RecordingConverter::convertRecording(
	drawdance::Project &project,
	drawdance::ProjectMessageCompressor &projectMessageCompressor, int i,
	long long &timeMsec, QString &outErrorMessage, QString &outDetail)
{
	const QString &path = m_inputs[i].path;
	QString basename = getBasename(path);
	Q_EMIT conversionProgressMessage(
		QStringLiteral("Converting recording %1…").arg(basename));

	drawdance::Player player;
	if(!player.open(DP_PLAYER_TYPE_GUESS, path)) {
		outErrorMessage =
			QStringLiteral("Failed to open recording %1.").arg(basename);
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	if(!player.isCompatible()) {
		outErrorMessage = QStringLiteral("Incompatible recording.");
		return false;
	}

	if(isCancelled()) {
		return false;
	}

	int result = project.openNewSession(
		DP_PROJECT_SOURCE_FILE, DP_PROJECT_SESSION_FLAG_CONVERTED);
	if(result != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 opening project session.").arg(result);
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	if(!projectMessageCompressor.setSessionId(project.sessionId())) {
		outErrorMessage = QStringLiteral("Error setting compressor session.");
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	long long snapshotId = project.openSnapshot(
		DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT |
		DP_PROJECT_SNAPSHOT_FLAG_CONVERTED);
	if(snapshotId <= 0LL) {
		outErrorMessage = QStringLiteral("Error %1 opening project snapshot.")
							  .arg(snapshotId);
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	result = project.finishSnapshot(snapshotId);
	if(result != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 finishing project snapshot.").arg(result);
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	drawdance::Playback playback;
	if(m_saveSnapshots) {
		playback.open();
	}

	bool nextHasTime = true;
	bool isOldRecording =
		player.compatibility() == DP_PLAYER_BACKWARD_COMPATIBLE;
	QVector<net::Message> msgs;
	msgs.reserve(2);
	while(true) {
		if(isCancelled()) {
			return false;
		}

		net::Message msg;
		DP_PlayerResult playerResult = player.step(true, msg);
		if(playerResult == DP_PLAYER_SUCCESS) {
			if(nextHasTime) {
				timeMsec += msg.guessMsecs(nextHasTime);
			} else {
				nextHasTime = true;
			}

			if(shouldRecordMessage(msg)) {
				// Old recordings have draw dabs messages that get converted to
				// have more parameters and larger layer id sizes, which can
				// lead to them being larger than the maximum size and so need
				// to be split up. Cutting them in half is fine, they can't get
				// nearly that much bigger.
				if(isOldRecording &&
				   msg.payloadLength() > DP_MESSAGE_MAX_PAYLOAD_LENGTH) {
					switch(msg.type()) {
					case DP_MSG_DRAW_DABS_CLASSIC:
						splitDrawDabsClassic(msg, msgs);
						break;
					case DP_MSG_DRAW_DABS_PIXEL:
					case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
						splitDrawDabsPixel(msg, msgs);
						break;
					case DP_MSG_DRAW_DABS_MYPAINT:
						splitDrawDabsMyPaint(msg, msgs);
						break;
					default:
						qWarning(
							"Message of type %d is too large to serialize",
							int(msg.type()));
						break;
					}
				} else {
					msgs.append(msg);
				}

				for(const net::Message &msgToRecord : msgs) {
					result = projectMessageCompressor.recordMessage(
						double(timeMsec) / 1000.0, msgToRecord,
						DP_PROJECT_MESSAGE_FLAG_CONVERTED);
					if(result != 0) {
						outErrorMessage =
							QStringLiteral("Error %1 converting recording %2.")
								.arg(result)
								.arg(basename);
						outDetail = QString::fromUtf8(DP_error());
						return false;
					}

					if(playback.get()) {
						playback.pushMessage(msgToRecord);
					}
				}

				msgs.clear();
			}

		} else if(playerResult == DP_PLAYER_RECORDING_END) {
			break;

		} else if(playerResult == DP_PLAYER_ERROR_PARSE) {
			qWarning("Error parsing recording message: %s", DP_error());

		} else {
			outErrorMessage = QStringLiteral("Error %1 reading recording %2.")
								  .arg(int(playerResult))
								  .arg(basename);
			outDetail = QString::fromUtf8(DP_error());
			return false;
		}

		emitConversionProgress(i, player.progress());
	}

	if(!projectMessageCompressor.flush()) {
		outErrorMessage = QStringLiteral("Error flushing compressor.");
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	if(playback.get()) {
		playback.flushMultidab();
		drawdance::CanvasState canvasState = playback.localCanvasState();
		// If this is the last recording, save a full snapshot at the end.
		// Otherwise just put a thumbnail on the session, which saving the
		// snapshot also does implicitly.
		int inputCount = m_inputs.size();
		if(i < inputCount - 1) {
			int thumbnailResult = project.setSessionThumbnail(
				canvasState, playback.drawContext());
			if(thumbnailResult != 0) {
				qWarning(
					"Error %d saving session thumbnail for %s", thumbnailResult,
					qUtf8Printable(path));
			}
		} else {
			int saveResult = project.saveSession(canvasState);
			if(saveResult != 0) {
				outErrorMessage =
					QStringLiteral("Error %1 saving project snapshot.")
						.arg(result);
				outDetail = QString::fromUtf8(DP_error());
				return false;
			}
		}
	}

	result = project.closeSession();
	if(result != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 closing project session.").arg(result);
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	return true;
}

bool RecordingConverter::fixupProject(
	drawdance::Project &project, const QString &path, InputProject *ip, int i,
	QString &outErrorMessage, QString &outDetail)
{
	int continuedSessionCount = ip->continuedSessions.size();
	ip->sortContinuedSessions();

	drawdance::Project sourceProject;
	drawdance::ProjectPlayer projectPlayer;
	QString basename = getBasename(path);
	auto openSourceProject = [&] {
		if(!sourceProject.get()) {
			int openError;
			if(!sourceProject.open(
				   ip->tempFile.fileName(), DP_PROJECT_OPEN_EXISTING,
				   &openError)) {
				outErrorMessage =
					QStringLiteral("Error %1 opening project %2.")
						.arg(QString::number(openError), basename);
				outDetail = QString::fromUtf8(DP_error());
				return false;
			}
		}
		return true;
	};

	for(int j = 0; j < continuedSessionCount; ++j) {
		const ContinuedSession &s = ip->continuedSessions[j];
		Q_EMIT conversionProgressMessage(
			QStringLiteral("Processing project %1 snapshot %2…")
				.arg(basename, QString::number(s.targetSnapshotId)));

		if(s.orphaned) {
			if(!projectPlayer.get()) {
				if(!openSourceProject()) {
					return false;
				}

				if(!projectPlayer.open(sourceProject)) {
					outErrorMessage =
						QStringLiteral("Error opening playback for project %1.")
							.arg(basename);
					outDetail = QString::fromUtf8(DP_error());
					return false;
				}

				int prepareResult = projectPlayer.prepare(0.01, 0LL);
				if(prepareResult != 0) {
					outErrorMessage =
						QStringLiteral(
							"Error %1 preparing playback for project %2.")
							.arg(QString::number(prepareResult), basename);
					outDetail = QString::fromUtf8(DP_error());
					return false;
				}
			}

			drawdance::CanvasState canvasState;
			int controlResult = projectPlayer.controlSeekIds(
				1u, s.continueSessionId, s.continueSequenceId,
				[this, i, &projectPlayer, &canvasState](
					DP_ProjectPlayer *, const DP_ProjectPlayerControlParams &,
					int type) {
					if(type == DP_PROJECT_PLAYER_CONTROL_CALLBACK_PROGRESS) {
						if(isCancelled()) {
							return DP_PROJECT_PLAYER_PROGRESS_CANCEL;
						}

						double total = projectPlayer.totalPlaybackSeconds();
						double progress;
						if(total == 0.0) {
							progress = 1.0;
						} else {
							progress =
								projectPlayer.currentPlaybackSeconds() / total;
						}
						emitFixupProgress(i, progress);

						return DP_PROJECT_PLAYER_PROGRESS_CONTINUE;

					} else if(
						type == DP_PROJECT_PLAYER_CONTROL_CALLBACK_UPDATE) {
						canvasState = projectPlayer.currentLocalCanvasState();
						return 0;

					} else {
						qWarning(
							"Unhandled project player control type %d", type);
						return 0;
					}
				});

			if(isCancelled()) {
				return false;
			}

			if(controlResult != 0) {
				outErrorMessage =
					QStringLiteral("Error %1 processing project %2.")
						.arg(QString::number(controlResult), basename);
				outDetail = QString::fromUtf8(DP_error());
				return false;
			}

			int replaceResult = project.copySessionFixReplaceSnapshot(
				s.targetSnapshotId, s.sequenceId, canvasState);
			if(replaceResult != 0) {
				outErrorMessage =
					QStringLiteral("Error %1 replacing snapshot in project %2.")
						.arg(QString::number(replaceResult), basename);
				outDetail = QString::fromUtf8(DP_error());
				return false;
			}

		} else {
			QHash<long long, long long>::const_iterator it =
				ip->sessionIdMapping.constFind(s.continueSessionId);
			// This shouldn't happen, we checked if it's present earlier.
			if(it == ip->sessionIdMapping.constEnd()) {
				outErrorMessage =
					QStringLiteral("Session %1 not found in project %2.")
						.arg(QString::number(s.continueSessionId), basename);
				return false;
			}

			int fixResult = project.copySessionFixSnapshotContinuedSessionId(
				s.targetSnapshotId, it.value());
			if(fixResult != 0) {
				outErrorMessage =
					QStringLiteral("Error %1 updating snapshot in project %2.")
						.arg(QString::number(fixResult), basename);
				outDetail = QString::fromUtf8(DP_error());
				return false;
			}
		}

		if(isCancelled()) {
			return false;
		}

		emitFixupProgress(i, double(j) / double(continuedSessionCount));
	}

	const Input &lastInput = m_inputs.constLast();
	if(lastInput.project && lastInput.path == path) {
		if(!openSourceProject()) {
			return false;
		}

		long long lastSessionId;
		int lastSessionIdResult =
			sourceProject.copySessionFixLastSessionId(lastSessionId);
		if(lastSessionIdResult != 0) {
			outErrorMessage =
				QStringLiteral("Error %1 reading sessions in project %2.")
					.arg(QString::number(lastSessionIdResult), basename);
			outDetail = QString::fromUtf8(DP_error());
			return false;
		}

		if(lastInput.sessionId != lastSessionId) {
			m_needsFinalReplay = true;
		}
	}

	return true;
}

bool RecordingConverter::makeFinalSnapshot(
	drawdance::Project &project, int i, QString &outErrorMessage,
	QString &outDetail)
{
	Q_EMIT conversionProgressMessage(
		QStringLiteral("Processing final project snapshot…"));

	drawdance::ProjectPlayer projectPlayer;
	if(!projectPlayer.open(project)) {
		outErrorMessage =
			QStringLiteral("Error opening playback for project %1")
				.arg(m_tempFile->path());
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	int prepareResult = projectPlayer.prepare(0.01, 0LL);
	if(prepareResult != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 preparing playback for project %2")
				.arg(QString::number(prepareResult), m_tempFile->path());
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	drawdance::CanvasState canvasState;
	long long sessionId = 0LL;
	long long sequenceId = 0LL;
	int controlResult = projectPlayer.controlFastForward(
		1u, [this, i, &projectPlayer, &canvasState, &sequenceId](
				DP_ProjectPlayer *, const DP_ProjectPlayerControlParams &,
				int type) {
			if(type == DP_PROJECT_PLAYER_CONTROL_CALLBACK_PROGRESS) {
				if(isCancelled()) {
					return DP_PROJECT_PLAYER_PROGRESS_CANCEL;
				}

				double total = projectPlayer.totalPlaybackSeconds();
				double progress;
				if(total == 0.0) {
					progress = 1.0;
				} else {
					progress = projectPlayer.currentPlaybackSeconds() / total;
				}
				emitFixupProgress(i, progress);

				return DP_PROJECT_PLAYER_PROGRESS_CONTINUE;

			} else if(type == DP_PROJECT_PLAYER_CONTROL_CALLBACK_UPDATE) {
				canvasState = projectPlayer.currentLocalCanvasState();
				sequenceId = projectPlayer.currentSequenceId();
				return 0;

			} else {
				qWarning("Unhandled project player control type %d", type);
				return 0;
			}
		});

	if(isCancelled()) {
		return false;
	}

	if(controlResult != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 processing project %2.")
				.arg(QString::number(controlResult), m_tempFile->path());
		outDetail = QString::fromUtf8(DP_error());
		return false;
	}

	projectPlayer.close();

	int saveResult = project.saveSessionAt(sessionId, sequenceId, canvasState);
	if(saveResult != 0) {
		outErrorMessage =
			QStringLiteral("Error %1 saving snapshot in project %2.")
				.arg(QString::number(saveResult), m_tempFile->path());
		outDetail = QString::fromUtf8(DP_error());
	}

	return true;
}

void RecordingConverter::emitConversionProgress(int i, double progress)
{
	int percent;
	if(m_progressInputCount > 1) {
		double percentPerInput = 100.0 / double(m_progressInputCount);
		percent = int(
			(double(i) * percentPerInput) + (progress * percentPerInput) + 0.5);
	} else {
		percent = qRound(progress * 100.0);
	}

	if(percent < 0) {
		percent = 0;
	} else if(percent > 100) {
		percent = 100;
	}

	if(percent != m_lastPercent) {
		m_lastPercent = percent;
		Q_EMIT conversionProgress(percent);
	}
}

void RecordingConverter::emitFixupProgress(int i, double progress)
{
	emitConversionProgress(m_inputs.size() + i, progress);
}

QString RecordingConverter::getBasename(const QString &path)
{
	QHash<QString, QString>::const_iterator it =
		m_basenameByPath.constFind(path);
	if(it == m_basenameByPath.constEnd()) {
		QString basename = io::PathInfo(path).basename();
		m_basenameByPath.insert(path, basename);
		return basename;
	} else {
		return *it;
	}
}

bool RecordingConverter::shouldRecordMessage(const net::Message &msg)
{
	if(msg.isInControlRange()) {
		return false;
	}

	switch(msg.type()) {
	case DP_MSG_INTERVAL:
		return false;
	default:
		return true;
	}
}
}
