// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/export/canvassaverrunnable.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/annotation.h"
#include "libclient/drawdance/global.h"
#include "libclient/utils/annotations.h"

CanvasSaverRunnable::CanvasSaverRunnable(
	const drawdance::CanvasState &canvasState, DP_SaveImageType type,
	const QString &path, QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_type(type)
	, m_path(path.toUtf8())
{
}

void CanvasSaverRunnable::run()
{
	const char *path = m_path.constData();
	qDebug("Saving to '%s'", path);
	drawdance::DrawContext dc = drawdance::DrawContextPool::acquire();
	DP_SaveResult result = DP_save(
		m_canvasState.get(), dc.get(), m_type, path, bakeAnnotation, this);
	emit saveComplete(saveResultToErrorString(result));
}

QString CanvasSaverRunnable::saveResultToErrorString(DP_SaveResult result)
{
	switch(result) {
	case DP_SAVE_RESULT_SUCCESS:
	case DP_SAVE_RESULT_CANCEL:
		return QString{};
	case DP_SAVE_RESULT_BAD_ARGUMENTS:
		return tr("Bad arguments, this is probably a bug in Drawpile.");
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
		annotation.text(), annotation.valign());
	return true;
}
