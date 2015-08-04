/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include <QDebug>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QQuickView>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QVBoxLayout>
#include <QApplication>

#include "dialogs/playbackdialog.h"
#include "dialogs/videoexportdialog.h"
#include "dialogs/recfilterdialog.h"

#include "recording/playbackcontroller.h"

#include "mainwindow.h"

#include "../shared/record/reader.h"
#include "../shared/net/recording.h"

#include "utils/iconprovider.h"

namespace dialogs {

PlaybackDialog::PlaybackDialog(canvas::CanvasModel *canvas, recording::Reader *reader, QWidget *parent) :
	QDialog(parent), m_closing(false)
{
	// Note: we contain the QtQuick view inside a widget based dialog, since
	// we must open subdialogs (filtering, video export, file selection) and
	// opening nested dialogs in QML leads to weird behaviour (Qt 5.5.0)

	setWindowTitle(tr("Playback"));
	setWindowFlags(Qt::Tool);
	setMinimumSize(200, 80);
	resize(420, 250);

	// All GUI-agnostic stuff is in PlaybackController. In the future when QtQuick dialogs
	// work properly, we can easily get rid of this wrapping QDialog.
	m_ctrl = new recording::PlaybackController(canvas, reader, this);
	connect(m_ctrl, &recording::PlaybackController::playbackToggled, this, &PlaybackDialog::playbackToggled);
	connect(m_ctrl, &recording::PlaybackController::commandRead, this, &PlaybackDialog::commandRead);

	m_view = new QQuickView;
	m_view->setResizeMode(QQuickView::SizeRootObjectToView);
	m_view->engine()->addImportPath(":/qml/");
	m_view->engine()->addImageProvider(QStringLiteral("theme"), new icon::IconProvider);

	m_view->rootContext()->setContextProperty("playback", m_ctrl);
	m_view->rootContext()->setContextProperty("dialog", this);

	m_view->setSource(QUrl("qrc:/qml/PlaybackDialog.qml"));

	QWidget *container = QWidget::createWindowContainer(m_view, this);
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(container);
}

PlaybackDialog::~PlaybackDialog()
{
	delete m_view;
}

void PlaybackDialog::centerOnParent()
{
	if(parentWidget() != 0) {
		QRect parentG = parentWidget()->geometry();
		QRect myG = geometry();

		move(
			parentG.x() + parentG.width()/2 - myG.width()/2,
			parentG.y() + parentG.height()/2 - myG.height()/2
		);
	}
}

bool PlaybackDialog::isPlaying() const
{
	return m_ctrl->isPlaying();
}

recording::Reader *PlaybackDialog::openRecording(const QString &filename, QWidget *msgboxparent)
{
	QScopedPointer<recording::Reader> reader(new recording::Reader(filename));

	recording::Compatibility result = reader->open();

	QString warning;
	bool fatal=false;

	switch(result) {
	using namespace recording;
	case COMPATIBLE: break;
	case MINOR_INCOMPATIBILITY:
		warning = tr("This recording was made with a different Drawpile version (%1) and may appear differently").arg(reader->writerVersion());
		break;
	case UNKNOWN_COMPATIBILITY:
		warning = tr("This recording was made with a newer Drawpile version (%1) which might not be compatible").arg(reader->writerVersion());
		break;
	case INCOMPATIBLE:
		warning = tr("Recording is incompatible. This recording was made with Drawpile version %1.").arg(reader->writerVersion());
		fatal = true;
		break;
	case NOT_DPREC:
		warning = tr("Selected file is not a Drawpile recording");
		fatal = true;
		break;
	case CANNOT_READ:
		warning = tr("Cannot read file: %1").arg(reader->errorString());
		fatal = true;
		break;
	}

	if(!warning.isNull()) {
		if(fatal) {
			QMessageBox::warning(msgboxparent, tr("Open Recording"), warning);
			return 0;
		} else {
			int res = QMessageBox::warning(msgboxparent, tr("Open Recording"), warning, QMessageBox::Ok, QMessageBox::Cancel);
			if(res != QMessageBox::Ok)
				return 0;
		}
	}

	return reader.take();
}

void PlaybackDialog::closeEvent(QCloseEvent *event)
{
	if(!exitCleanup())
		event->ignore();
	else
		QDialog::closeEvent(event);
}

void PlaybackDialog::keyPressEvent(QKeyEvent *event)
{
	// This is not a OK/Cancel type dialog, so disable
	// key events. Without this, it is easy to close
	// the window accidentally by hitting Esc.
	event->ignore();
}

void PlaybackDialog::done(int r)
{
	if(exitCleanup())
		QDialog::done(r);
}

void PlaybackDialog::filterRecording()
{
	dialogs::FilterRecordingDialog dlg(this);

	// Get entries to silence
	dlg.setSilence(m_ctrl->getSilencedEntries());
	dlg.setNewMarkers(m_ctrl->getNewMarkers());

	if(dlg.exec() == QDialog::Accepted) {
		QString filename = dlg.filterRecording(m_ctrl->recordingFilename());

		if(!filename.isEmpty()) {
			MainWindow *win = new MainWindow(false);
			win->open(QUrl::fromLocalFile(filename));
		}
	}
}

void PlaybackDialog::configureVideoExport()
{
	QScopedPointer<VideoExportDialog> dialog(new VideoExportDialog(this));

	VideoExporter *ve=0;
	while(!ve) {
		if(dialog->exec() != QDialog::Accepted)
			return;

		ve = dialog->getExporter();
	}

	m_ctrl->startVideoExport(ve);
}

void PlaybackDialog::addMarker()
{
	bool ok;
	QString title = QInputDialog::getText(this, tr("Mark Position"), tr("Marker text"), QLineEdit::Normal, QString(), &ok);
	if(ok) {
		m_ctrl->addMarker(title);
	}
}

bool PlaybackDialog::exitCleanup()
{
	if(m_ctrl->isExporting()) {
		if(!m_closing) {
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			m_closing = true;
			connect(m_ctrl, &recording::PlaybackController::exportEnded, this, &QDialog::close);
			m_ctrl->stopExporter();
		}
		return false;
	} else {
		if(m_closing)
			QApplication::restoreOverrideCursor();
		return true;
	}
}

}
