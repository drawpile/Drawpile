// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/brushpresetproperties.h"
#include "ui_brushpresetproperties.h"
#include <QFileDialog>
#include <QPainter>


namespace dialogs {

BrushPresetProperties::BrushPresetProperties(
	int id, const QString &name, const QString &description,
	const QPixmap &thumbnail, QWidget *parent)
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
	connect(
		m_ui->chooseThumbnailButton, &QPushButton::clicked, this,
		&BrushPresetProperties::chooseThumbnailFile);
	connect(
		m_ui->label, &QLineEdit::textChanged, this,
		&BrushPresetProperties::renderThumbnail);
	connect(
		m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(
		m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(
		this, &QDialog::accepted, this, &BrushPresetProperties::emitChanges);
}

BrushPresetProperties::~BrushPresetProperties()
{
	delete m_ui;
}

void BrushPresetProperties::chooseThumbnailFile()
{
	QString file = QFileDialog::getOpenFileName(
		this, tr("Select brush thumbnail"), QString(),
		"Images (*.png *.jpg *.jpeg)");

	QPixmap pixmap;
	if(!file.isEmpty() && pixmap.load(file)) {
		showThumbnail(pixmap);
	}
}

void BrushPresetProperties::emitChanges()
{
	QString label = m_ui->label->text();
	emit presetPropertiesApplied(
		m_id, m_ui->name->text(), m_ui->description->toPlainText(),
		label.trimmed().isEmpty() ? m_thumbnail : applyThumbnailLabel(label));
}

void BrushPresetProperties::showThumbnail(const QPixmap &thumbnail)
{
	m_thumbnail = thumbnail.scaled(
		64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	renderThumbnail();
}

void BrushPresetProperties::renderThumbnail()
{
	QGraphicsScene *scene = m_ui->thumbnail->scene();
	scene->clear();
	scene->setSceneRect(QRect(0, 0, 64, 64));
	QString label = m_ui->label->text();
	scene->addPixmap(
		label.trimmed().isEmpty() ? m_thumbnail : applyThumbnailLabel(label));
}

QPixmap BrushPresetProperties::applyThumbnailLabel(const QString &label)
{
	QPixmap thumbnail = m_thumbnail;
	QPainter painter(&thumbnail);
	qreal h = m_thumbnail.height();
	qreal y = h * 3.0 / 4.0;
	QRectF rect(0, y, m_thumbnail.width(), h - y);
	painter.fillRect(rect, palette().window());
	painter.setPen(palette().windowText().color());
	painter.drawText(
		rect, label, QTextOption(Qt::AlignHCenter | Qt::AlignBaseline));
	return thumbnail;
}

}
