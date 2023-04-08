// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/resizedialog.h"
#include "libclient/utils/images.h"

#include "ui_resizedialog.h"

#include <QMessageBox>
#include <QPushButton>

namespace dialogs {

ResizeDialog::ResizeDialog(const QSize &oldsize, QWidget *parent) :
	QDialog(parent), m_oldsize(oldsize), m_aspectratio(0), m_lastchanged(0)
{
	m_ui = new Ui_ResizeDialog;
	m_ui->setupUi(this);

	m_ui->resizer->setOriginalSize(oldsize);
	m_ui->resizer->setTargetSize(oldsize);

	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Resize"));

	QPushButton *centerButton = new QPushButton(tr("Center"));
	m_ui->buttons->addButton(centerButton, QDialogButtonBox::ActionRole);

	m_ui->width->setValue(m_oldsize.width());
	m_ui->height->setValue(m_oldsize.height());

	connect(m_ui->width, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeDialog::widthChanged);
	connect(m_ui->height, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResizeDialog::heightChanged);
	connect(m_ui->keepaspect, &QCheckBox::toggled, this, &ResizeDialog::toggleAspectRatio);

	connect(centerButton, &QPushButton::clicked, m_ui->resizer, &widgets::ResizerWidget::center);
	connect(m_ui->buttons->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this, &ResizeDialog::reset);
}

ResizeDialog::~ResizeDialog()
{
	delete m_ui;
}

void ResizeDialog::setPreviewImage(const QImage &image)
{
	m_ui->resizer->setImage(image);
}

void ResizeDialog::setBounds(const QRect &rect)
{
	auto rectIn = rect.intersected({{0,0}, m_ui->resizer->originalSize()});

	m_aspectratio = rectIn.width() / float(rectIn.height());

	m_ui->width->setValue(rectIn.width());
	m_ui->height->setValue(rectIn.height());

	m_ui->resizer->setOffset(-rectIn.topLeft());
	m_ui->resizer->setTargetSize(rectIn.size());
}

void ResizeDialog::done(int r)
{
	if(r == QDialog::Accepted) {
		if(!utils::checkImageSize(newSize())) {
			QMessageBox::information(this, tr("Error"), tr("Size is too large"));
			return;
		}
	}

	QDialog::done(r);
}

void ResizeDialog::widthChanged(int newWidth)
{
	if(m_aspectratio) {
		m_ui->height->blockSignals(true);
		m_ui->height->setValue(newWidth / m_aspectratio);
		m_ui->height->blockSignals(false);
	}
	m_ui->resizer->setTargetSize(QSize(newWidth, m_ui->height->value()));
	m_lastchanged = 0;
}

void ResizeDialog::heightChanged(int newHeight)
{
	if(m_aspectratio) {
		m_ui->width->blockSignals(true);
		m_ui->width->setValue(newHeight * m_aspectratio);
		m_ui->width->blockSignals(false);
	}
	m_ui->resizer->setTargetSize(QSize(m_ui->width->value(), newHeight));
	m_lastchanged = 1;
}

void ResizeDialog::toggleAspectRatio(bool keep)
{
	if(keep) {
		m_aspectratio = m_oldsize.width() / float(m_oldsize.height());

		if(m_lastchanged==0)
			widthChanged(m_ui->width->value());
		else
			heightChanged(m_ui->height->value());

	} else {
		m_aspectratio = 0;
	}
}

void ResizeDialog::reset()
{
	m_ui->width->blockSignals(true);
	m_ui->height->blockSignals(true);
	m_ui->width->setValue(m_oldsize.width());
	m_ui->height->setValue(m_oldsize.height());
	m_ui->width->blockSignals(false);
	m_ui->height->blockSignals(false);
	m_ui->resizer->setTargetSize(m_oldsize);
}

QSize ResizeDialog::newSize() const
{
	return QSize(m_ui->width->value(), m_ui->height->value());
}

QPoint ResizeDialog::newOffset() const
{
	return m_ui->resizer->offset();
}

ResizeVector ResizeDialog::resizeVector() const
{
	ResizeVector rv;
	QSize s = newSize();
	QPoint o = newOffset();

	rv.top = o.y();
	rv.left = o.x();
	rv.right = s.width() - m_oldsize.width() - o.x();
	rv.bottom = s.height() - m_oldsize.height() - o.y();

	return rv;
}

}
