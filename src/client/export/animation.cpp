/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "animation.h"
#include "videoexporter.h"
#include "dialogs/videoexportdialog.h"
#include "core/layerstack.h"
#include "core/layer.h"

#include <QProgressDialog>
#include <QMessageBox>

AnimationExporter::AnimationExporter(paintcore::LayerStack *layers, VideoExporter *exporter, QObject *parent)
	: QObject(parent), _layers(layers), _exporter(exporter),
	  _currentFrame(1)
{
	connect(_exporter, &VideoExporter::exporterReady, this, &AnimationExporter::saveNextFrame, Qt::QueuedConnection);
	connect(_exporter, &VideoExporter::exporterError, this, &AnimationExporter::error);
	connect(_exporter, &VideoExporter::exporterFinished, this, &AnimationExporter::done);
}

void AnimationExporter::start()
{
	_exporter->start();
}

void AnimationExporter::saveNextFrame()
{
	if(_currentFrame > _endFrame) {
		_exporter->finish();

	} else {
		_layers->lock();
		QImage image = _layers->flatLayerImage(_currentFrame - 1, _useBgLayer, _bgColor);
		_layers->unlock();

		_exporter->saveFrame(image, 1);

		emit progress(_currentFrame);
		_currentFrame++;
	}
}

void AnimationExporter::exportAnimation(paintcore::LayerStack *layers, QWidget *parent)
{
	auto *dlg = new dialogs::VideoExportDialog(parent);
	dlg->showAnimationSettings(layers->layerCount());
	dlg->show();

	connect(dlg, &QDialog::finished, [layers, dlg, parent](int result) {
		if(result == QDialog::Accepted) {
			VideoExporter *vexp = dlg->getExporter();
			if(vexp) {
				auto *exporter = new AnimationExporter(layers, vexp, parent);
				vexp->setParent(exporter);
				connect(exporter, &AnimationExporter::done, exporter, &AnimationExporter::deleteLater);

				connect(exporter, &AnimationExporter::error, [parent](const QString &msg) {
					QMessageBox::warning(parent, tr("Export error"), msg);
				});

				exporter->_startFrame = dlg->getFirstLayer();
				exporter->_endFrame = dlg->getLastLayer();
				exporter->_currentFrame = exporter->_startFrame;
				exporter->_useBgLayer = dlg->useBackgroundLayer();
				exporter->_bgColor = dlg->animationBackground();

				auto *pdlg = new QProgressDialog(tr("Exporting..."), tr("Cancel"), 1, 10, parent);
				pdlg->setWindowModality(Qt::WindowModal);
				pdlg->setAttribute(Qt::WA_DeleteOnClose);

				connect(exporter, &AnimationExporter::progress, pdlg, &QProgressDialog::setValue);
				connect(exporter, &AnimationExporter::done, pdlg, &QProgressDialog::close);
				pdlg->show();

				exporter->start();
			}
		}

		dlg->deleteLater();
	});
}
