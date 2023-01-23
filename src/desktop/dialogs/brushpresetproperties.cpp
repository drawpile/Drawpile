/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 askmeaboutloom

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

#include "desktop/dialogs/brushpresetproperties.h"

#include "ui_brushpresetproperties.h"

#include <QFileDialog>


namespace dialogs {

BrushPresetProperties::BrushPresetProperties(int id, const QString &name,
		const QString &description, const QPixmap &thumbnail, QWidget *parent)
	: QDialog(parent)
	, m_id(id)
	, m_thumbnail()
{
    m_ui = new Ui_BrushPresetProperties;
    m_ui->setupUi(this);

	m_ui->name->setText(name);
	m_ui->description->setPlainText(description);

	QGraphicsScene *scene = new QGraphicsScene(this);
	m_ui->thumbnail->setScene(scene);
	showThumbnail(thumbnail);

    connect(m_ui->name, &QLineEdit::returnPressed, this, &QDialog::accept);
	connect(m_ui->chooseThumbnailButton, &QPushButton::pressed,
		this, &BrushPresetProperties::chooseThumbnailFile);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this, &QDialog::accepted, this, &BrushPresetProperties::emitChanges);
}

BrushPresetProperties::~BrushPresetProperties()
{
	delete m_ui;
}

void BrushPresetProperties::chooseThumbnailFile()
{
	QString file = QFileDialog::getOpenFileName(this,
		tr("Select brush thumbnail"), QString(),
		"Images (*.png *.jpg *.jpeg)");

	QPixmap pixmap;
	if(!file.isEmpty() && pixmap.load(file)) {
		showThumbnail(pixmap);
	}
}

void BrushPresetProperties::emitChanges()
{
	emit presetPropertiesApplied(
		m_id, m_ui->name->text(), m_ui->description->toPlainText(), m_thumbnail);
}

void BrushPresetProperties::showThumbnail(const QPixmap &thumbnail)
{
	m_thumbnail = thumbnail.scaled(
		64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QGraphicsScene *scene = m_ui->thumbnail->scene();
	scene->clear();
	scene->setSceneRect(QRect(0, 0, 64, 64));
	scene->addPixmap(m_thumbnail);
}

}
