// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/layerproperties.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/net/message.h"
#include "ui_layerproperties.h"
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStyle>
#include <functional>

namespace dialogs {

LayerProperties::LayerProperties(uint8_t localUser, QWidget *parent)
	: QDialog(parent)
	, m_user(localUser)
{
	m_ui = new Ui_LayerProperties;
	m_ui->setupUi(this);

	m_ui->sketchTintPreview->setDisplayMode(
		color_widgets::ColorPreview::AllAlpha);
	m_ui->sketchTintPreview->setCursor(Qt::PointingHandCursor);

	m_ui->sketchTintNoneButton->setGroupPosition(
		widgets::GroupedToolButton::GroupLeft);
	m_ui->sketchTintNoneButton->setIcon(
		utils::makeColorIconFor(this, Qt::transparent));

	m_ui->sketchTintBlueButton->setGroupPosition(
		widgets::GroupedToolButton::GroupCenter);
	m_ui->sketchTintBlueButton->setIcon(
		utils::makeColorIconFor(this, LAYER_SKETCH_TINT_DEFAULT));

	m_ui->sketchTintChangeButton->setGroupPosition(
		widgets::GroupedToolButton::GroupRight);

	connect(m_ui->title, &QLineEdit::returnPressed, this, &QDialog::accept);
	connect(
		m_ui->sketchMode, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&LayerProperties::updateSketchMode);
	connect(
		m_ui->sketchTintPreview, &color_widgets::ColorPreview::clicked, this,
		&LayerProperties::showSketchTintColorPicker);
	connect(
		m_ui->sketchTintNoneButton, &widgets::GroupedToolButton::clicked, this,
		std::bind(&LayerProperties::setSketchTintTo, this, Qt::transparent));
	connect(
		m_ui->sketchTintBlueButton, &widgets::GroupedToolButton::clicked, this,
		std::bind(
			&LayerProperties::setSketchTintTo, this,
			LAYER_SKETCH_TINT_DEFAULT));
	connect(
		m_ui->sketchTintChangeButton, &widgets::GroupedToolButton::clicked,
		this, &LayerProperties::showSketchTintColorPicker);
	connect(
		m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(
		m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(
		m_ui->buttonBox, &QDialogButtonBox::clicked, this,
		[this](QAbstractButton *b) {
			if(m_item.id != 0 &&
			   m_ui->buttonBox->buttonRole(b) == QDialogButtonBox::ApplyRole) {
				apply();
			}
		});
	connect(this, &QDialog::accepted, this, &LayerProperties::apply);
	updateSketchMode(Qt::Unchecked);
}

LayerProperties::~LayerProperties()
{
	delete m_ui;
}

void LayerProperties::setNewLayerItem(
	int selectedId, bool group, const QString &title)
{
	m_item = {
		0,	   QString(), 1.0f,	 DP_BLEND_MODE_NORMAL,
		0.0f,  QColor(),  false, false,
		false, group,	  group, 0,
		0,	   0,		  0,
	};
	m_selectedId = selectedId;
	m_wasDefault = false;
	setWindowTitle(group ? tr("New Layer Group") : tr("New Layer"));
	m_ui->title->setText(title);
	m_ui->opacitySlider->setValue(100);
	m_ui->visible->setChecked(true);
	m_ui->censored->setChecked(false);
	setSketchParamsFromSettings();
	m_ui->defaultLayer->setChecked(false);
	m_ui->createdByLabel->hide();
	m_ui->createdBy->hide();
	updateBlendMode(
		m_ui->blendMode, m_item.blend, m_item.group, m_item.isolated,
		m_compatibilityMode);
	// Can't apply settings to a layer that doesn't exist yet.
	QPushButton *applyButton = m_ui->buttonBox->button(QDialogButtonBox::Apply);
	if(applyButton) {
		applyButton->setEnabled(false);
		applyButton->hide();
	}
}

void LayerProperties::setLayerItem(
	const canvas::LayerListItem &item, const QString &creator, bool isDefault)
{
	m_item = item;
	m_wasDefault = isDefault;

	m_ui->title->setText(item.title);
	m_ui->opacitySlider->setValue(qRound(item.opacity * 100.0f));
	m_ui->visible->setChecked(!item.hidden);
	m_ui->censored->setChecked(item.actuallyCensored());
	if(item.sketchOpacity <= 0.0f) {
		setSketchParamsFromSettings();
	} else {
		m_ui->sketchMode->setChecked(true);
		m_ui->sketchOpacitySlider->setValue(
			qRound(item.sketchOpacity * 100.0f));
		m_ui->sketchTintPreview->setColor(item.sketchTint);
	}
	m_ui->defaultLayer->setChecked(isDefault);
	m_ui->createdBy->setText(creator);
	updateBlendMode(
		m_ui->blendMode, item.blend, item.group, item.isolated,
		m_compatibilityMode);
}

void LayerProperties::updateLayerItem(
	const canvas::LayerListItem &item, const QString &creator, bool isDefault)
{
	m_item = item;
	m_wasDefault = isDefault;
	m_ui->createdBy->setText(creator);
}

void LayerProperties::setControlsEnabled(bool enabled)
{
	m_controlsEnabled = enabled;
	QWidget *w[] = {
		m_ui->title, m_ui->opacitySlider, m_ui->blendMode, m_ui->censored};
	for(unsigned int i = 0; i < sizeof(w) / sizeof(w[0]); ++i)
		w[i]->setEnabled(enabled);
}

void LayerProperties::setOpControlsEnabled(bool enabled)
{
	m_ui->defaultLayer->setEnabled(enabled);
}

void LayerProperties::setCompatibilityMode(bool compatibilityMode)
{
	int blendModeData = m_ui->blendMode->currentData().toInt();
	DP_BlendMode mode =
		blendModeData == -1 ? m_item.blend : DP_BlendMode(blendModeData);
	m_compatibilityMode = compatibilityMode;
	updateBlendMode(
		m_ui->blendMode, mode, m_item.group, m_item.isolated,
		m_compatibilityMode);
}

void LayerProperties::updateBlendMode(
	QComboBox *combo, DP_BlendMode mode, bool group, bool isolated,
	bool compatibilityMode)
{
	if(compatibilityMode) {
		combo->setModel(compatibilityLayerBlendModes());
	} else if(group) {
		combo->setModel(groupBlendModes());
	} else {
		combo->setModel(layerBlendModes());
	}
	// If this is an unknown blend mode, hide the control to avoid damage.
	int blendModeIndex = searchBlendModeIndex(combo, mode);
	combo->setVisible(blendModeIndex != -1);
	combo->setCurrentIndex(group && !isolated ? 0 : blendModeIndex);
}

QStandardItemModel *LayerProperties::layerBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(model);
	}
	return model;
}

QStandardItemModel *LayerProperties::groupBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		QStandardItem *item = new QStandardItem{tr("Pass Through")};
		item->setData(-1, Qt::UserRole);
		model->appendRow(item);
		addBlendModesTo(model);
	}
	return model;
}

QStandardItemModel *LayerProperties::compatibilityLayerBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(model);
		canvas::blendmode::setCompatibilityMode(model, true);
	}
	return model;
}

void LayerProperties::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	m_ui->title->setFocus(Qt::PopupFocusReason);
	m_ui->title->selectAll();
}

void LayerProperties::updateSketchMode(compat::CheckBoxState state)
{
	bool visible = state != Qt::Unchecked;
	m_ui->sketchOpacityLabel->setVisible(visible);
	m_ui->sketchOpacitySlider->setVisible(visible);
	m_ui->sketchTintLabel->setVisible(visible);
	m_ui->sketchTintWidget->setVisible(visible);
}

void LayerProperties::showSketchTintColorPicker()
{
	color_widgets::ColorDialog *dlg = dialogs::newDeleteOnCloseColorDialog(
		m_ui->sketchTintPreview->color(), this);
	dlg->setAlphaEnabled(true);
	dlg->setPreviewDisplayMode(color_widgets::ColorPreview::AllAlpha);
	connect(
		dlg, &color_widgets::ColorDialog::colorSelected,
		m_ui->sketchTintPreview, &color_widgets::ColorPreview::setColor);
	dlg->show();
}

void LayerProperties::setSketchTintTo(const QColor &color)
{
	m_ui->sketchTintPreview->setColor(color);
}

void LayerProperties::setSketchParamsFromSettings()
{
	const desktop::settings::Settings &settings = dpApp().settings();
	m_ui->sketchMode->setChecked(false);
	m_ui->sketchOpacitySlider->setValue(settings.layerSketchOpacityPercent());
	m_ui->sketchTintPreview->setColor(settings.layerSketchTint());
}

void LayerProperties::saveSketchParametersToSettings(
	int opacityPercent, const QColor &tint)
{
	if(opacityPercent > 0) {
		desktop::settings::Settings &settings = dpApp().settings();
		settings.setLayerSketchOpacityPercent(opacityPercent);
		settings.setLayerSketchTint(tint);
	}
}

void LayerProperties::apply()
{
	if(m_item.id == 0) {
		int blendModeData = m_ui->blendMode->currentData().toInt();
		int sketchOpacity = m_ui->sketchMode->isChecked()
								? m_ui->sketchOpacitySlider->value()
								: 0;
		QColor sketchTint =
			sketchOpacity <= 0 ? QColor() : m_ui->sketchTintPreview->color();
		saveSketchParametersToSettings(sketchOpacity, sketchTint);
		emit addLayerOrGroupRequested(
			m_selectedId, m_item.group, m_ui->title->text(),
			m_ui->opacitySlider->value(),
			blendModeData == -1 ? DP_BLEND_MODE_NORMAL : blendModeData,
			m_item.group && blendModeData != -1, m_ui->censored->isChecked(),
			m_ui->defaultLayer->isChecked(), m_ui->visible->isChecked(),
			sketchOpacity, sketchTint);
	} else {
		emitChanges();
	}
}

void LayerProperties::emitChanges()
{
	net::MessageList messages;

	QString title = m_ui->title->text();
	if(m_item.title != title) {
		messages.append(net::makeLayerRetitleMessage(m_user, m_item.id, title));
	}

	const int oldOpacity = qRound(m_item.opacity * 100.0);
	const bool censored = m_ui->censored->isChecked();
	DP_BlendMode newBlendmode;
	bool isolated;
	if(m_ui->blendMode->isEnabled()) {
		int blendModeData = m_ui->blendMode->currentData().toInt();
		if(blendModeData == -1) {
			newBlendmode = m_item.blend;
			isolated = false;
		} else {
			newBlendmode = DP_BlendMode(blendModeData);
			isolated = m_item.group;
		}
	} else {
		newBlendmode = m_item.blend;
		isolated = m_item.isolated;
	}

	if(m_ui->opacitySlider->value() != oldOpacity ||
	   newBlendmode != m_item.blend || censored != m_item.actuallyCensored() ||
	   isolated != m_item.isolated) {
		uint8_t flags = (censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0) |
						(isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED : 0);
		uint8_t opacity = qRound(m_ui->opacitySlider->value() / 100.0 * 255);
		messages.append(net::makeLayerAttributesMessage(
			m_user, m_item.id, 0, flags, opacity, newBlendmode));
	}

	if(m_ui->visible->isChecked() != (!m_item.hidden)) {
		emit visibilityChanged(m_item.id, m_ui->visible->isChecked());
	}

	int oldSketchOpacity = qRound(m_item.sketchOpacity * 100.0);
	int sketchOpacity =
		m_ui->sketchMode->isChecked() ? m_ui->sketchOpacitySlider->value() : 0;
	QColor oldSketchTint = m_item.sketchTint;
	QColor sketchTint =
		sketchOpacity <= 0 ? QColor() : m_ui->sketchTintPreview->color();
	if(sketchOpacity != oldSketchOpacity ||
	   (sketchOpacity > 0 && oldSketchTint != sketchTint)) {
		saveSketchParametersToSettings(sketchOpacity, sketchTint);
		emit sketchModeChanged(m_item.id, sketchOpacity, sketchTint);
	}

	bool makeDefault = m_ui->defaultLayer->isChecked();
	if(m_ui->defaultLayer->isEnabled() && makeDefault != m_wasDefault) {
		messages.append(
			net::makeDefaultLayerMessage(m_user, makeDefault ? m_item.id : 0));
	}

	if(!messages.isEmpty()) {
		messages.prepend(net::makeUndoPointMessage(m_user));
		emit layerCommands(messages.count(), messages.constData());
	}
}

void LayerProperties::addBlendModesTo(QStandardItemModel *model)
{
	for(const canvas::blendmode::Named &m :
		canvas::blendmode::layerModeNames()) {
		QStandardItem *item = new QStandardItem{m.name};
		item->setData(int(m.mode), Qt::UserRole);
		model->appendRow(item);
	}
}

int LayerProperties::searchBlendModeIndex(QComboBox *combo, DP_BlendMode mode)
{
	int blendModeCount = combo->count();
	for(int i = 0; i < blendModeCount; ++i) {
		if(combo->itemData(i).toInt() == int(mode)) {
			return i;
		}
	}
	return -1;
}

}
