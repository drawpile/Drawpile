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

#include "flipbook.h"
#include "core/layerstack.h"
#include "utils/icon.h"

#include "ui_flipbook.h"

#include <QSettings>
#include <QRect>
#include <QTimer>
#include <QDesktopWidget>
#include <QApplication>

namespace dialogs {

Flipbook::Flipbook(QWidget *parent)
	: QDialog(parent), _ui(new Ui_Flipbook), _layers(nullptr)
{
	_ui->setupUi(this);

	_timer = new QTimer(this);

	connect(_ui->useBgLayer, &QCheckBox::toggled, [this](bool usebg) {
		_ui->layerIndex->setMinimum(usebg ? 2 : 1);
		resetFrameCache();
		loadFrame();
	});

	connect(_ui->rewindButton, &QToolButton::clicked, this, &Flipbook::rewind);
	connect(_ui->playButton, &QToolButton::clicked, this, &Flipbook::playPause);
	connect(_ui->layerIndex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Flipbook::loadFrame);
	connect(_ui->fps, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &Flipbook::updateFps);
	connect(_timer, &QTimer::timeout, _ui->layerIndex, &QSpinBox::stepUp);

	_ui->playButton->setFocus();

	// Load default settings
	QSettings cfg;
	cfg.beginGroup("flipbook");

	_ui->fps->setValue(cfg.value("fps", 15).toInt());
	_ui->useBgLayer->setChecked(cfg.value("bglayer", true).toBool());

	QRect geom = cfg.value("window", QRect()).toRect();
	if(geom.isValid()) {
		setGeometry(geom);
	}

	_ui->layerIndex->setMinimum(_ui->useBgLayer->isChecked() ? 2 : 1);
}

Flipbook::~Flipbook()
{
	// Save settings
	QSettings cfg;
	cfg.beginGroup("flipbook");

	cfg.setValue("fps", _ui->fps->value());
	cfg.setValue("bglayer", _ui->useBgLayer->isChecked());
	cfg.setValue("window", geometry());

	delete _ui;
}

void Flipbook::rewind()
{
	_ui->layerIndex->setValue(_ui->layerIndex->minimum());
}

void Flipbook::playPause()
{
	if(_timer->isActive()) {
		_timer->stop();
		_ui->playButton->setIcon(icon::fromTheme("media-playback-start"));

	} else {
		_timer->start(1000 / _ui->fps->value());
		_ui->playButton->setIcon(icon::fromTheme("media-playback-pause"));
	}
}

void Flipbook::updateFps(int newFps)
{
	if(_timer->isActive()) {
		_timer->setInterval(1000 / newFps);
	}
}

void Flipbook::setLayers(paintcore::LayerStack *layers)
{
	Q_ASSERT(layers);
	_layers = layers;
	_ui->layerIndex->setMaximum(_layers->layers());
	_ui->layerIndex->setSuffix(QStringLiteral("/%1").arg(_layers->layers()));

	resetFrameCache();
	loadFrame();
}

void Flipbook::resetFrameCache()
{
	_frames.clear();
	if(_layers) {
		for(int i=0;i<_layers->layers();++i)
			_frames.append(QPixmap());
	}
}

void Flipbook::loadFrame()
{
	const int f = _ui->layerIndex->value() - 1;
	if(_layers && f < _frames.size()) {
		if(_frames.at(f).isNull()) {
			QImage img = _layers->flatLayerImage(f, _ui->useBgLayer->isChecked(), QColor(0,0,0,0));

			// Scale down the image if it is too big
			QSize maxSize = qApp->desktop()->availableGeometry(this).size() * 0.7;
			if(img.width() > maxSize.width() || img.height() > maxSize.height()) {
				QSize newSize = QSize(img.width(), img.height()).boundedTo(maxSize);
				img = img.scaled(newSize);
			}

			_frames[f] = QPixmap::fromImage(img);
		}

		_ui->pixmap->setPixmap(_frames.at(f));
	} else
		_ui->pixmap->setPixmap(QPixmap());
}

}
