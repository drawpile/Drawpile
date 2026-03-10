// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/create.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/resizedialog.h"
#include "desktop/main.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/utils/images.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QtColorWidgets/ColorPreview>

using color_widgets::ColorDialog;
using color_widgets::ColorPreview;

namespace dialogs {
namespace startdialog {

Create::Create(QWidget *parent)
	: Page{parent}
{
	QFormLayout *layout = new QFormLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	QHBoxLayout *widthLayout = new QHBoxLayout;
	layout->addRow(tr("Width:"), widthLayout);
	m_widthSpinner = new KisSliderSpinBox;
	m_widthSpinner->setIndeterminate(true);
	m_widthSpinner->setRange(1, 9999999);
	m_widthSpinner->setFastSliderStep(10);
	widthLayout->addWidget(m_widthSpinner);
	widthLayout->addWidget(new QLabel(tr("px")), 1);

	QHBoxLayout *heightLayout = new QHBoxLayout;
	layout->addRow(tr("Height:"), heightLayout);
	m_heightSpinner = new KisSliderSpinBox;
	m_heightSpinner->setIndeterminate(true);
	m_heightSpinner->setRange(1, 9999999);
	m_heightSpinner->setFastSliderStep(10);
	heightLayout->addWidget(m_heightSpinner);
	heightLayout->addWidget(new QLabel(tr("px")), 1);

	DrawpileApp &app = dpApp();
	QSize lastSize = app.safeNewCanvasSize();
	bool lastSizeValid =
		lastSize.isValid() && ResizeDialog::checkDimensions(
								  lastSize.width(), lastSize.height(), false);
	m_widthSpinner->setValue(lastSizeValid ? lastSize.width() : 1920);
	m_heightSpinner->setValue(lastSizeValid ? lastSize.height() : 1080);

	m_backgroundPreview =
		makeBackgroundPreview(app.config()->getNewCanvasBackColor());
	layout->addRow(tr("Background:"), m_backgroundPreview);

	m_errorLabel = new QLabel;
	m_errorLabel->setTextFormat(Qt::PlainText);
	m_errorLabel->setVisible(false);
	m_errorLabel->setWordWrap(true);
	layout->addRow(m_errorLabel);

	connect(
		m_widthSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &Create::updateCreateButton);
	connect(
		m_heightSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &Create::updateCreateButton);
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
	config::Config *cfg = dpAppConfig();
	cfg->setNewCanvasSize(size);
	cfg->setNewCanvasBackColor(backgroundColor);
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
		m_widthSpinner->value(), m_heightSpinner->value(), false, &error);
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
