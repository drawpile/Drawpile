// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/shadeselectordialog.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/noscroll.h"
#include "desktop/widgets/shadeselector.h"
#include "libclient/config/config.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtColorWidgets/ColorWheel>

namespace dialogs {

ShadeSelectorDialog::ShadeSelectorDialog(const QColor &color, QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Color Harmonies"));
	utils::makeModal(this);
	resize(400, 600);

	config::Config *cfg = dpAppConfig();
	m_colorSpace = cfg->getColorWheelSpace();

	QVBoxLayout *layout = new QVBoxLayout(this);

	m_colorShadesEnabledBox =
		new QCheckBox(tr("Show harmony swatches under color wheel"));
	m_colorShadesEnabledBox->setChecked(cfg->getColorShadesEnabled());
	layout->addWidget(m_colorShadesEnabledBox);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_stack, 1);

	QWidget *disabledWidget = new QWidget;
	disabledWidget->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(disabledWidget);

	QVBoxLayout *disabledLayout = new QVBoxLayout(disabledWidget);
	disabledLayout->setContentsMargins(0, 0, 0, 0);
	disabledLayout->addStretch();

	QLabel *disabledLabel = new QLabel(
		QStringLiteral("<p>%1</p><p><a href=\"#\">%2</a></p>")
			.arg(
				tr("Color harmony swatches are disabled.").toHtmlEscaped(),
				//: "Them" refers to the color harmony swatches.
				tr("Click here to enable them.").toHtmlEscaped()));
	disabledLabel->setTextFormat(Qt::RichText);
	disabledLabel->setAlignment(Qt::AlignCenter);
	disabledLayout->addWidget(disabledLabel);
	connect(
		disabledLabel, &QLabel::linkActivated, m_colorShadesEnabledBox,
		&QCheckBox::click);

	disabledLayout->addStretch();

	QWidget *configWidget = new QWidget;
	configWidget->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(configWidget);

	QVBoxLayout *configLayout = new QVBoxLayout(configWidget);
	configLayout->setContentsMargins(0, 0, 0, 0);

	m_rowsConfig = cfg->getColorShadesConfig();
	if(m_rowsConfig.isEmpty()) {
		m_rowsConfig = widgets::ShadeSelector::getDefaultConfig();
	}

	m_rowCountSpinner = new KisSliderSpinBox;
	m_rowCountSpinner->setRange(1, 10);
	m_rowCountSpinner->setPrefix(tr("Rows: "));
	m_rowCountSpinner->setValue(m_rowsConfig.size());
	configLayout->addWidget(m_rowCountSpinner);
	connect(
		m_rowCountSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &ShadeSelectorDialog::setRowCount);

	m_rowHeightSpinner = new KisSliderSpinBox;
	m_rowHeightSpinner->setRange(4, 32);
	m_rowHeightSpinner->setValue(cfg->getColorShadesRowHeight());
	m_rowHeightSpinner->setPrefix(tr("Row height: "));
	m_rowHeightSpinner->setSuffix(tr("px"));
	configLayout->addWidget(m_rowHeightSpinner);

	m_columnCountSpinner = new KisSliderSpinBox;
	m_columnCountSpinner->setRange(2, 50);
	m_columnCountSpinner->setValue(cfg->getColorShadesColumnCount());
	m_columnCountSpinner->setPrefix(tr("Colors per row: "));
	configLayout->addWidget(m_columnCountSpinner);

	m_borderThicknessSpinner = new KisSliderSpinBox;
	m_borderThicknessSpinner->setRange(0, 8);
	m_borderThicknessSpinner->setValue(cfg->getColorShadesBorderThickness());
	m_borderThicknessSpinner->setPrefix(tr("Border: "));
	configLayout->addWidget(m_borderThicknessSpinner);

	m_shadeSelector = new widgets::ShadeSelector;
	m_shadeSelector->setColor(color);
	m_shadeSelector->setConfig(m_rowsConfig);
	m_shadeSelector->setRowHeight(m_rowHeightSpinner->value());
	m_shadeSelector->setColumnCount(m_columnCountSpinner->value());
	m_shadeSelector->setBorderThickness(m_borderThicknessSpinner->value());
	configLayout->addWidget(m_shadeSelector);
	connect(
		m_rowHeightSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		m_shadeSelector, &widgets::ShadeSelector::setRowHeight);
	connect(
		m_columnCountSpinner,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), m_shadeSelector,
		&widgets::ShadeSelector::setColumnCount);
	connect(
		m_borderThicknessSpinner,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), m_shadeSelector,
		&widgets::ShadeSelector::setBorderThickness);
	connect(
		m_shadeSelector, &widgets::ShadeSelector::colorDoubleClicked,
		m_shadeSelector, &widgets::ShadeSelector::forceSetColor);

	m_scroll = new QScrollArea;
	m_scroll->setWidgetResizable(true);
	utils::bindKineticScrolling(m_scroll);
	configLayout->addWidget(m_scroll, 1);

	QWidget *rowHolderWidget = new QWidget;
	m_scroll->setWidget(rowHolderWidget);
	rowHolderWidget->setBackgroundRole(QPalette::Base);

	m_rowsLayout = new QVBoxLayout(rowHolderWidget);
	m_rowsLayout->addStretch(1);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(0);
	layout->addLayout(buttonLayout);

	QPushButton *pickColorButton =
		new QPushButton(QIcon::fromTheme("color-picker"), tr("Set Color…"));
	connect(
		pickColorButton, &QPushButton::clicked, this,
		&ShadeSelectorDialog::pickColor);
	buttonLayout->addWidget(pickColorButton);

	buttonLayout->addStretch();

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttonLayout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(
		m_colorShadesEnabledBox, &QCheckBox::clicked, this,
		&ShadeSelectorDialog::setColorShadesEnabled);
	setColorShadesEnabled(m_colorShadesEnabledBox->isChecked());
	setRowCount(m_rowsConfig.size());

	int count = qMin(m_rowsConfig.size(), m_rowWidgets.size());
	for(int i = 0; i < count; ++i) {
		m_rowWidgets[i]->setFromComfig(m_rowsConfig[i].toHash());
	}

	connect(
		this, &ShadeSelectorDialog::accepted, this,
		&ShadeSelectorDialog::saveToSettings);
}

ShadeSelectorDialog::RowWidget::RowWidget(ShadeSelectorDialog *dlg, int index)
	: QFrame()
	, m_dlg(dlg)
{
	setAutoFillBackground(true);
	setBackgroundRole(QPalette::Window);
	setFrameShape(QFrame::Box);
	setFrameShadow(QFrame::Raised);
	QVBoxLayout *layout = new QVBoxLayout(this);

	m_combo = new widgets::NoScrollComboBox;
	m_combo->addItem(
		m_dlg->tr("Custom"), int(widgets::ShadeSelector::Preset::None));
	m_combo->addItem(
		m_dlg->tr("Hue"), int(widgets::ShadeSelector::Preset::HueRange));
	m_combo->addItem(
		dlg->getColorSpaceText2(m_dlg->tr("Saturation"), m_dlg->tr("Chroma")),
		int(widgets::ShadeSelector::Preset::SaturationRange));
	m_combo->addItem(
		dlg->getColorSpaceText3(
			m_dlg->tr("Value"), m_dlg->tr("Lightness"), m_dlg->tr("Luminance")),
		int(widgets::ShadeSelector::Preset::ValueRange));
	m_combo->addItem(
		m_dlg->tr("Shades"), int(widgets::ShadeSelector::Preset::Shades));
	m_combo->setCurrentIndex(0);
	layout->addWidget(m_combo);

	QHBoxLayout *moveLayout = new QHBoxLayout;
	moveLayout->setContentsMargins(0, 0, 0, 0);
	moveLayout->setSpacing(0);
	layout->addLayout(moveLayout);

	m_moveUpButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_moveUpButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_moveUpButton->setIcon(QIcon::fromTheme("arrow-up"));
	m_moveUpButton->setToolTip(m_dlg->tr("Move Up"));
	moveLayout->addWidget(m_moveUpButton);
	connect(
		m_moveUpButton, &widgets::GroupedToolButton::clicked, m_dlg,
		[this, index] {
			m_dlg->swapRows(index, index - 1);
		});

	m_moveDownButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_moveDownButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_moveDownButton->setIcon(QIcon::fromTheme("arrow-down"));
	m_moveDownButton->setToolTip(m_dlg->tr("Move Down"));
	moveLayout->addWidget(m_moveDownButton);
	connect(
		m_moveDownButton, &widgets::GroupedToolButton::clicked, m_dlg,
		[this, index] {
			m_dlg->swapRows(index, index + 1);
		});

	moveLayout->addStretch();

	connect(
		m_combo, QOverload<int>::of(&QComboBox::activated), this,
		&RowWidget::updateSliders);
	updateSliders(m_combo->currentIndex());
}

void ShadeSelectorDialog::RowWidget::setFromComfig(
	const QVariantHash &rowConfig)
{
	QVariantList preset = rowConfig.value(QStringLiteral("p")).toList();
	if(!preset.isEmpty()) {
		int type = preset[0].toInt();
		int comboCount = m_combo->count();
		for(int i = 1; i < comboCount; ++i) {
			if(m_combo->itemData(i).toInt() == type) {
				m_combo->setCurrentIndex(i);
				updateSliders(i);
				int minCount = qMin(m_sliders.size(), preset.size() - 1);
				for(int j = 0; j < minCount; ++j) {
					m_sliders[j]->setValue(
						qRound(preset[j + 1].toReal() * 100.0));
				}
				return;
			}
		}
	}

	m_combo->setCurrentIndex(0);
	updateSliders(0);
	QStringList keys = getCustomConfigKeys();
	int minCount = qMin(m_sliders.size(), keys.size());
	for(int j = 0; j < minCount; ++j) {
		m_sliders[j]->setValue(
			qRound(rowConfig.value(keys[j]).toReal() * 100.0));
	}
}

QVariantHash ShadeSelectorDialog::RowWidget::toConfig() const
{
	QVariantHash rowConfig;
	QStringList keys = getCustomConfigKeys();

	if(m_preset == 0) {
		int minCount = qMin(m_sliders.size(), keys.size());
		for(int i = 0; i < minCount; ++i) {
			rowConfig.insert(keys[i], m_sliders[i]->value() / 100.0);
		}
	} else {
		QVariantList preset;
		preset.append(m_preset);
		for(const KisSliderSpinBox *slider : m_sliders) {
			preset.append(slider->value() / 100.0);
		}
		rowConfig.insert(QStringLiteral("p"), preset);

		QVector<qreal> resolvedValues =
			widgets::ShadeSelector::configToRow(rowConfig).toVector();
		int minCount = qMin(resolvedValues.size(), keys.size());
		for(int i = 0; i < minCount; ++i) {
			rowConfig.insert(keys[i], resolvedValues[i]);
		}
	}

	return rowConfig;
}

void ShadeSelectorDialog::RowWidget::setMovable(bool up, bool down)
{
	m_moveUpButton->setEnabled(up);
	m_moveDownButton->setEnabled(down);
}

void ShadeSelectorDialog::RowWidget::swapWith(RowWidget *rw)
{
	purgeSliders();
	rw->purgeSliders();
	qSwap(m_preset, rw->m_preset);
	int comboIndex = m_combo->currentIndex();
	m_combo->setCurrentIndex(rw->m_combo->currentIndex());
	rw->m_combo->setCurrentIndex(comboIndex);
	m_stashedSliderValues.swap(rw->m_stashedSliderValues);
	initSliders();
	rw->initSliders();
	m_dlg->updateConfig();
}

void ShadeSelectorDialog::RowWidget::updateSliders(int index)
{
	int preset = m_combo->itemData(index).toInt();
	if(preset != m_preset) {
		purgeSliders();
		m_preset = preset;
		initSliders();
		m_dlg->updateConfig();
	}
}

void ShadeSelectorDialog::RowWidget::purgeSliders()
{
	int count = m_sliders.size();
	for(int i = 0; i < count; ++i) {
		stashSliderValue(m_preset, i);
		m_sliders[i]->hide();
		m_sliders[i]->deleteLater();
	}
	m_sliders.clear();
}

void ShadeSelectorDialog::RowWidget::initSliders()
{
	switch(m_preset) {
	case int(widgets::ShadeSelector::Preset::HueRange):
		addSlider(m_dlg->tr("Range: "), 10);
		break;
	case int(widgets::ShadeSelector::Preset::SaturationRange):
	case int(widgets::ShadeSelector::Preset::ValueRange):
		addSlider(m_dlg->tr("Range: "), 100);
		break;
	case int(widgets::ShadeSelector::Preset::Shades):
		addSlider(
			m_dlg->getColorSpaceText2(
				m_dlg->tr("Saturation range: "), m_dlg->tr("Chroma range: ")),
			100);
		addSlider(
			m_dlg->getColorSpaceText3(
				m_dlg->tr("Value range: "), m_dlg->tr("Lightness range: "),
				m_dlg->tr("Luminance range: ")),
			100);
		break;
	default:
		addSlider(tr("Hue range: "), 100);
		addSlider(
			m_dlg->getColorSpaceText2(
				m_dlg->tr("Saturation range: "), m_dlg->tr("Chroma range: ")),
			0);
		addSlider(
			m_dlg->getColorSpaceText3(
				m_dlg->tr("Value range: "), m_dlg->tr("Lightness range: "),
				m_dlg->tr("Luminance range: ")),
			0);
		addSlider(m_dlg->tr("Hue offset: "), 0);
		addSlider(
			m_dlg->getColorSpaceText2(
				m_dlg->tr("Saturation offset: "), m_dlg->tr("Chroma offset: ")),
			0);
		addSlider(
			m_dlg->getColorSpaceText3(
				m_dlg->tr("Value offset: "), m_dlg->tr("Lightness offset: "),
				m_dlg->tr("Luminance offset: ")),
			0);
		break;
	}
}

void ShadeSelectorDialog::RowWidget::addSlider(
	const QString &prefix, int defaultValue)
{
	int i = m_sliders.size();
	KisSliderSpinBox *slider = new widgets::NoScrollKisSliderSpinBox;
	slider->setPrefix(prefix);
	slider->setSuffix(m_dlg->tr("%"));
	slider->setRange(-100, 100);
	slider->setValue(unstashSliderValue(m_preset, i, defaultValue));
	QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(layout());
	vl->insertWidget(vl->count() - 1, slider);
	m_sliders.append(slider);
	connect(
		slider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), m_dlg,
		&ShadeSelectorDialog::updateConfig);
}

void ShadeSelectorDialog::RowWidget::stashSliderValue(
	int preset, int sliderIndex)
{
	int key = getStashKey(preset, sliderIndex);
	m_stashedSliderValues.insert(key, m_sliders[sliderIndex]->value());
}

int ShadeSelectorDialog::RowWidget::unstashSliderValue(
	int preset, int sliderIndex, int defaultValue) const
{
	int key = getStashKey(preset, sliderIndex);
	if(m_stashedSliderValues.contains(key)) {
		return m_stashedSliderValues.value(key);
	} else {
		return defaultValue;
	}
}

int ShadeSelectorDialog::RowWidget::getStashKey(int preset, int sliderIndex)
{
	return preset * 100 + sliderIndex;
}

QStringList ShadeSelectorDialog::RowWidget::getCustomConfigKeys()
{
	return {QStringLiteral("rh"), QStringLiteral("rs"), QStringLiteral("rv"),
			QStringLiteral("sh"), QStringLiteral("ss"), QStringLiteral("sv")};
}

void ShadeSelectorDialog::updateConfig()
{
	m_shadeSelector->setConfig(getConfig());
}

QVariantList ShadeSelectorDialog::getConfig() const
{
	QVariantList rowsConfig;
	rowsConfig.reserve(m_rowWidgets.size());
	for(const RowWidget *rw : m_rowWidgets) {
		rowsConfig.append(rw->toConfig());
	}
	return rowsConfig;
}

void ShadeSelectorDialog::swapRows(int i, int j)
{
	int count = m_rowWidgets.size();
	if(i != j && i >= 0 && j >= 0 && i < count && j < count) {
		m_rowWidgets[i]->swapWith(m_rowWidgets[j]);
	}
}

QString ShadeSelectorDialog::getColorSpaceText2(
	const QString &hsvHslText, const QString &lchText) const
{
	return getColorSpaceText3(hsvHslText, hsvHslText, lchText);
}

QString ShadeSelectorDialog::getColorSpaceText3(
	const QString &hsvText, const QString &hslText,
	const QString &lchText) const
{
	switch(m_colorSpace) {
	case int(color_widgets::ColorWheel::ColorHSL):
		return hslText;
	case int(color_widgets::ColorWheel::ColorLCH):
		return lchText;
	default:
		return hsvText;
	}
}

void ShadeSelectorDialog::setColorShadesEnabled(bool enabled)
{
	m_stack->setCurrentIndex(enabled ? 1 : 0);
}

void ShadeSelectorDialog::setRowCount(int rowCount)
{
	while(m_rowWidgets.size() < rowCount) {
		if(m_spareRowWidgets.isEmpty()) {
			RowWidget *rw = new RowWidget(this, m_rowWidgets.size());
			m_rowWidgets.append(rw);
			m_rowsLayout->insertWidget(m_rowWidgets.count() - 1, rw);
		} else {
			RowWidget *rw = m_spareRowWidgets.takeLast();
			m_rowWidgets.append(rw);
			rw->show();
		}
	}

	while(m_rowWidgets.size() > rowCount) {
		RowWidget *rw = m_rowWidgets.takeLast();
		m_spareRowWidgets.append(rw);
		rw->hide();
	}

	for(int i = 0; i < rowCount; ++i) {
		m_rowWidgets[i]->setMovable(i != 0, i != rowCount - 1);
	}

	updateConfig();
}

void ShadeSelectorDialog::pickColor()
{
	color_widgets::ColorDialog *dlg =
		newDeleteOnCloseColorDialog(m_shadeSelector->color(), this);
	dlg->setAlphaEnabled(false);
	utils::makeModal(dlg);
	connect(
		dlg, &color_widgets::ColorDialog::colorSelected, m_shadeSelector,
		&widgets::ShadeSelector::forceSetColor);
	utils::showWindow(dlg);
}

void ShadeSelectorDialog::saveToSettings()
{
	config::Config *cfg = dpAppConfig();
	bool enabled = m_colorShadesEnabledBox->isChecked();
	cfg->setColorShadesEnabled(enabled);
	if(enabled) {
		cfg->setColorShadesBorderThickness(m_borderThicknessSpinner->value());
		cfg->setColorShadesColumnCount(m_columnCountSpinner->value());
		cfg->setColorShadesConfig(getConfig());
		cfg->setColorShadesRowHeight(m_rowHeightSpinner->value());
	}
}

}
