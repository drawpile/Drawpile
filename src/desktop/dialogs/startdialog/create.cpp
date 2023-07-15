// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/create.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/utils/sanerformlayout.h"
#include "libclient/utils/images.h"
#include <QSpinBox>
#include <QtColorWidgets/ColorPreview>

using color_widgets::ColorDialog;
using color_widgets::ColorPreview;

namespace dialogs {
namespace startdialog {

Create::Create(QWidget *parent)
	: Page{parent}
{
	utils::SanerFormLayout *layout = new utils::SanerFormLayout{this};
	layout->setContentsMargins(0, 0, 0, 0);

	m_widthSpinner = new QSpinBox;
	m_widthSpinner->setSuffix(tr("px"));
	m_widthSpinner->setSingleStep(10);
	m_widthSpinner->setRange(1, INT16_MAX);
	m_widthSpinner->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addRow(tr("Width:"), m_widthSpinner, 1, 1, Qt::AlignLeft);

	m_heightSpinner = new QSpinBox;
	m_heightSpinner->setSuffix(tr("px"));
	m_heightSpinner->setSingleStep(10);
	m_heightSpinner->setRange(1, INT16_MAX);
	m_heightSpinner->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	layout->addRow(tr("Height:"), m_heightSpinner, 1, 1, Qt::AlignLeft);

	m_backgroundPreview = new ColorPreview;
	m_backgroundPreview->setDisplayMode(ColorPreview::DisplayMode::AllAlpha);
	m_backgroundPreview->setToolTip(tr("Canvas background color"));
	m_backgroundPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addRow(tr("Background:"), m_backgroundPreview, 1, 1, Qt::AlignLeft);

	const desktop::settings::Settings &settings = dpApp().settings();
	QSize lastSize = settings.newCanvasSize();
	bool lastSizeValid = lastSize.isValid() && utils::checkImageSize(lastSize);
	m_widthSpinner->setValue(lastSizeValid ? lastSize.width() : 1920);
	m_heightSpinner->setValue(lastSizeValid ? lastSize.height() : 1080);

	QColor lastColor = settings.newCanvasBackColor();
	m_backgroundPreview->setColor(lastColor.isValid() ? lastColor : Qt::white);

	connect(
		m_widthSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Create::updateCreateButton);
	connect(
		m_heightSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&Create::updateCreateButton);
	connect(
		m_backgroundPreview, &ColorPreview::clicked, this,
		&Create::showColorPicker);
}

void Create::activate()
{
	emit showButtons();
	updateCreateButton();
}

void Create::accept()
{
	QSize size{m_widthSpinner->value(), m_heightSpinner->value()};
	QColor backgroundColor = m_backgroundPreview->color();
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setNewCanvasSize(size);
	settings.setNewCanvasBackColor(backgroundColor);
	emit create(size, backgroundColor);
}

void Create::updateCreateButton()
{
	QSize size{m_widthSpinner->value(), m_heightSpinner->value()};
	emit enableCreate(size.isValid() && utils::checkImageSize(size));
}

void Create::showColorPicker()
{
	ColorDialog *dlg = dialogs::newDeleteOnCloseColorDialog(
		m_backgroundPreview->color(), this);
	connect(
		dlg, &ColorDialog::colorSelected, m_backgroundPreview,
		&ColorPreview::setColor);
	dlg->show();
}

}
}
