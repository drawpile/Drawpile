// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpimpex/save.h>
}
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/annotation.h"
#include "libclient/drawdance/global.h"
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/utils/annotations.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QRandomGenerator>
#include <QTemporaryDir>
#ifdef Q_OS_ANDROID
#	include <QDateTime>
#	include <QDir>
#	include <QFile>
#endif

CanvasSaverRunnable::CanvasSaverRunnable(
	const drawdance::CanvasState &canvasState, DP_SaveImageType type,
	const QString &path, const canvas::PaintEngine *paintEngine,
	QTemporaryDir *tempDir, QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_type(type)
	, m_path(path)
	, m_tempDir(tempDir)
{
	if(paintEngine && DP_save_image_type_is_flat_image(type)) {
		m_vmb = new drawdance::ViewModeBuffer;
		m_vmf = paintEngine->viewModeFilter(*m_vmb, m_canvasState);
	}
}

CanvasSaverRunnable::~CanvasSaverRunnable()
{
	delete m_vmb;
	delete m_tempDir;
}

void CanvasSaverRunnable::run()
{
	QElapsedTimer timer;
	timer.start();

#ifdef Q_OS_ANDROID
	// Android doesn't support QSaveFile and KArchive doesn't support the
	// Truncate mode required on Android to not leave potential garbage at the
	// end of the file written to, so we have to jigger this stuff ourselves.
	QString tempPath =
		QDir::temp().filePath(QStringLiteral("save_%1_%2")
								  .arg(QDateTime::currentMSecsSinceEpoch())
								  .arg(QRandomGenerator::global()->generate()));
	QByteArray pathBytes = tempPath.toUtf8();
#else
	QByteArray pathBytes = m_path.toUtf8();
#endif

	const char *path = pathBytes.constData();
	drawdance::DrawContext dc = drawdance::DrawContextPool::acquire();
	DP_SaveResult result = DP_save(
		m_canvasState.get(), dc.get(), m_type, path, m_vmb ? &m_vmf : nullptr,
		bakeAnnotation, this);

#ifdef Q_OS_ANDROID
	QFile tempFile(tempPath);
	if(result == DP_SAVE_RESULT_SUCCESS) {
		result = copyToTargetFile(tempFile);
	}
	if(!tempFile.remove()) {
		qWarning(
			"Error removing temporary file '%s': %s", path,
			qUtf8Printable(tempFile.errorString()));
	}
#endif

	emit saveComplete(saveResultToErrorString(result, m_type), timer.elapsed());
}

QString CanvasSaverRunnable::saveResultToErrorString(
	DP_SaveResult result, DP_SaveImageType type)
{
	switch(result) {
	case DP_SAVE_RESULT_SUCCESS:
	case DP_SAVE_RESULT_CANCEL:
		return QString{};
	case DP_SAVE_RESULT_BAD_ARGUMENTS:
		return tr("Bad arguments, this is probably a bug in Drawpile.");
	case DP_SAVE_RESULT_BAD_DIMENSIONS:
		return badDimensionsErrorString(
			DP_save_image_type_max_dimension(type),
			QString::fromUtf8(DP_save_image_type_name(type)));
	case DP_SAVE_RESULT_UNKNOWN_FORMAT:
		return tr("Unsupported format.");
	case DP_SAVE_RESULT_FLATTEN_ERROR:
		return tr("Couldn't merge the canvas into a flat image.");
	case DP_SAVE_RESULT_OPEN_ERROR:
		return tr("Couldn't open file for writing.");
	case DP_SAVE_RESULT_WRITE_ERROR:
		return tr("Save operation failed, but the file might have been "
				  "partially written.");
	case DP_SAVE_RESULT_INTERNAL_ERROR:
		return tr("Internal error during saving.");
	}
	return tr("Unknown error.");
}

QString CanvasSaverRunnable::badDimensionsErrorString(
	int maxDimension, const QString &format)
{
	//: %1 is a number and %2 is a file format. For example, the message
	//: will say "…must be between 1 and 65535 for JPEG."
	return tr("Canvas size out of bounds, width and height must be between 1 "
			  "and %1 for %2.")
		.arg(QString::number(maxDimension), format);
}

bool CanvasSaverRunnable::bakeAnnotation(
	void *user, DP_Annotation *a, unsigned char *out)
{
	Q_UNUSED(user);
	drawdance::Annotation annotation = drawdance::Annotation::inc(a);
	QImage img(
		out, annotation.width(), annotation.height(),
		QImage::Format_ARGB32_Premultiplied);
	img.fill(0);
	QPainter painter(&img);
	utils::paintAnnotation(
		&painter, annotation.size(), annotation.backgroundColor(),
		annotation.text(), annotation.alias(), annotation.valign());
	return true;
}

#ifdef Q_OS_ANDROID
DP_SaveResult CanvasSaverRunnable::copyToTargetFile(QFile &tempFile) const
{
	if(!tempFile.open(QIODevice::ReadOnly)) {
		DP_error_set(
			"Error opening temp file '%s': %s",
			qUtf8Printable(tempFile.fileName()),
			qUtf8Printable(tempFile.errorString()));
		return DP_SAVE_RESULT_OPEN_ERROR;
	}

	QFile targetFile(m_path);
	if(!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		DP_error_set(
			"Error opening target file '%s': %s",
			qUtf8Printable(targetFile.fileName()),
			qUtf8Printable(targetFile.errorString()));
		tempFile.close();
		return DP_SAVE_RESULT_OPEN_ERROR;
	}

	QByteArray buffer;
	buffer.resize(BUFSIZ);
	while(true) {
		qint64 read = tempFile.read(buffer.data(), BUFSIZ);
		if(read < 0) {
			DP_error_set(
				"Error reading from temporary file '%s': %s",
				qUtf8Printable(tempFile.fileName()),
				qUtf8Printable(tempFile.errorString()));
			tempFile.close();
			return DP_SAVE_RESULT_WRITE_ERROR;
		} else if(read > 0) {
			qint64 written = targetFile.write(buffer, read);
			if(written < 0) {
				DP_error_set(
					"Error writing %lld byte(s) to target file '%s': %s",
					static_cast<long long>(read),
					qUtf8Printable(targetFile.fileName()),
					qUtf8Printable(targetFile.errorString()));
				tempFile.close();
				return DP_SAVE_RESULT_WRITE_ERROR;
			} else if(written != read) {
				DP_error_set(
					"Tried to write %lld byte(s) to target file '%s', but only "
					"wrote %lld",
					static_cast<long long>(read), qUtf8Printable(m_path),
					static_cast<long long>(written));
				tempFile.close();
				return DP_SAVE_RESULT_WRITE_ERROR;
			}
		} else {
			tempFile.close();
			if(targetFile.flush()) {
				return DP_SAVE_RESULT_SUCCESS;
			} else {
				DP_error_set(
					"Error flushing target file '%s': %s",
					qUtf8Printable(targetFile.fileName()),
					qUtf8Printable(targetFile.errorString()));
				return DP_SAVE_RESULT_WRITE_ERROR;
			}
		}
	}
}
#endif
