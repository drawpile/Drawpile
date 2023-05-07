// SPDX-License-Identifier: GPL-3.0-or-later

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
	connect(m_ui->chooseThumbnailButton, &QPushButton::clicked,
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
