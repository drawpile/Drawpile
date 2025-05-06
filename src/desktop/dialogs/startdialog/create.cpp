// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/create.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/resizedialog.h"
#include "desktop/main.h"
#include "libclient/utils/images.h"
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QtColorWidgets/ColorPreview>

using color_widgets::ColorDialog;
using color_widgets::ColorPreview;

namespace dialogs {
namespace startdialog {

Create::Create(QWidget *parent)
	: Page{parent}
{
	// TODO: make the form layout work with RTL or go back to a QFormLayout.
	setLayoutDirection(Qt::LeftToRight);
	QFormLayout *layout = new QFormLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	m_widthSpinner = new QSpinBox;
	m_widthSpinner->setSuffix(tr("px"));
	m_widthSpinner->setRange(1, 99999999);
	m_widthSpinner->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addRow(tr("Width:"), m_widthSpinner);

	m_heightSpinner = new QSpinBox;
	m_heightSpinner->setSuffix(tr("px"));
	m_heightSpinner->setRange(1, 99999999);
	m_heightSpinner->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	layout->addRow(tr("Height:"), m_heightSpinner);

	const DrawpileApp &app = dpApp();
	QSize lastSize = app.safeNewCanvasSize();
	bool lastSizeValid =
		lastSize.isValid() &&
		ResizeDialog::checkDimensions(lastSize.width(), lastSize.height());
	m_widthSpinner->setValue(lastSizeValid ? lastSize.width() : 1920);
	m_heightSpinner->setValue(lastSizeValid ? lastSize.height() : 1080);

	m_backgroundPreview =
		makeBackgroundPreview(app.settings().newCanvasBackColor());
	layout->addRow(tr("Background:"), m_backgroundPreview);

	m_errorLabel = new QLabel;
	m_errorLabel->setTextFormat(Qt::PlainText);
	m_errorLabel->setVisible(false);
	m_errorLabel->setWordWrap(true);
	layout->addRow(m_errorLabel);

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

color_widgets::ColorPreview *Create::makeBackgroundPreview(const QColor &color)
{
	ColorPreview *backgroundPreview = new ColorPreview;
	backgroundPreview->setDisplayMode(ColorPreview::DisplayMode::AllAlpha);
	backgroundPreview->setToolTip(tr("Canvas background color"));
	backgroundPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	backgroundPreview->setCursor(Qt::PointingHandCursor);
	backgroundPreview->setColor(color.isValid() ? color : Qt::white);
	return backgroundPreview;
}

void Create::updateCreateButton()
{
	QString error;
	bool ok = ResizeDialog::checkDimensions(
		m_widthSpinner->value(), m_heightSpinner->value(), &error);
	m_errorLabel->setText(error);
	m_errorLabel->setVisible(!ok);
	emit enableCreate(ok);
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
