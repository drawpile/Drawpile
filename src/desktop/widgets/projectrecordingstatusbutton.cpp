// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/projectrecordingstatusbutton.h"
#include "desktop/dialogs/projectrecordingsettingsdialog.h"
#include "libclient/canvas/canvasmodel.h"
#include "libshared/util/paths.h"
#include <QEvent>
#include <QIcon>

namespace widgets {

ProjectRecordingStatusButton::ProjectRecordingStatusButton(QWidget *parent)
	: QToolButton(parent)
{
	setToolButtonStyle(Qt::ToolButtonIconOnly);
	setAutoRaise(true);
	setCanvas(nullptr);
}

void ProjectRecordingStatusButton::setCanvas(canvas::CanvasModel *canvas)
{
	m_canvas = canvas;
	if(canvas) {
		setEnabled(true);
		setProjectRecordingActive(m_canvas->isProjectRecording());
		connect(
			canvas, &canvas::CanvasModel::projectRecordingStarted, this,
			&ProjectRecordingStatusButton::onProjectRecordingStarted);
		connect(
			canvas, &canvas::CanvasModel::projectRecordingStopped, this,
			&ProjectRecordingStatusButton::onProjectRecordingStopped);
	} else {
		setEnabled(false);
		setProjectRecordingActive(false);
	}
}

bool ProjectRecordingStatusButton::event(QEvent *e)
{
	if(e->type() == QEvent::ToolTip) {
		QString tip;
		if(m_canvas && m_canvas->isProjectRecording()) {
			size_t lastReportedSizeInBytes =
				m_canvas->projectRecordingLastReportedSizeInBytes();
			tip.append(
				tr("Autorecovery is enabled, file size is %1.")
					.arg(
						utils::paths::formatFileSize(lastReportedSizeInBytes)));

			size_t sizeLimitInBytes =
				m_canvas->projectRecordingSizeLimitInBytes();
			tip.append(
				dialogs::ProjectRecordingSettingsDialog::getLimitText(
					lastReportedSizeInBytes, sizeLimitInBytes));
		} else {
			tip.append(tr("Autorecovery is disabled."));
		}
		tip.append(tr(" Click to manage."));
		setToolTip(tip);
	}
	return QToolButton::event(e);
}

void ProjectRecordingStatusButton::onProjectRecordingStarted()
{
	setProjectRecordingActive(true);
}

void ProjectRecordingStatusButton::onProjectRecordingStopped()
{
	setProjectRecordingActive(false);
}

void ProjectRecordingStatusButton::setProjectRecordingActive(bool active)
{
	setIcon(
		QIcon::fromTheme(
			active ? QStringLiteral("backup")
				   : QStringLiteral("drawpile_backup_off")));
}

}
