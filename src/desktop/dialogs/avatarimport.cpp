/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "avatarimport.h"

#include "widgets/resizerwidget.h"
using widgets::ResizerWidget;

#include "ui_avatarimport.h"

namespace dialogs {

AvatarImport::AvatarImport(const QImage &source, QWidget *parent)
	: QDialog(parent), m_ui(new Ui_AvatarImport), m_originalImage(source)
{
	const int maxSize = qMin(source.width(), source.height());

	m_ui->setupUi(this);
	m_ui->resizer->setImage(source);
	m_ui->resizer->setOriginalSize(source.size());
	m_ui->resizer->setTargetSize(QSize { maxSize, maxSize });

	m_ui->sizeSlider->setMaximum(maxSize);
	m_ui->sizeSlider->setValue(maxSize);

	connect(m_ui->sizeSlider, &QSlider::valueChanged, this, [this](int value) {
		m_ui->resizer->setTargetSize(QSize { value, value });
	});
}

AvatarImport::~AvatarImport()
{
	delete m_ui;
}

QImage AvatarImport::croppedAvatar() const
{
	const QPoint offset = m_ui->resizer->offset();
	const QSize size = m_ui->resizer->targetSize();

	return m_originalImage.copy(
		-offset.x(),
		-offset.y(),
		size.width(),
		size.height()
	);
}

}
