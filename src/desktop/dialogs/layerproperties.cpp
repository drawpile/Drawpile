// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/layerproperties.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/net/message.h"
#include "ui_layerproperties.h"
#include <QButtonGroup>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScopedValueRollback>
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

	m_colorButtons = new QButtonGroup(this);
	const QVector<utils::MarkerColor> &markerColors = utils::markerColors();
	for(int i = 0, count = markerColors.size(); i < count; ++i) {
		widgets::GroupedToolButton *colorButton =
			new widgets::GroupedToolButton(
				i == 0			 ? widgets::GroupedToolButton::GroupLeft
				: i == count - 1 ? widgets::GroupedToolButton::GroupRight
								 : widgets::GroupedToolButton::GroupCenter);
		const utils::MarkerColor &mc = markerColors[i];
		colorButton->setIcon(utils::makeColorIconFor(colorButton, mc.color));
		colorButton->setToolTip(mc.name);
		colorButton->setCheckable(true);
		m_colorButtons->addButton(colorButton, int(mc.color.rgb() & 0xffffffu));
		m_ui->colorLayout->addWidget(colorButton);
	}

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
		m_ui->blendMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LayerProperties::updateAlphaBasedOnBlendMode);
	connect(
		m_ui->blendMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LayerProperties::updateAlphaTogglesBasedOnBlendMode);
	connect(
		m_ui->alphaGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&LayerProperties::updateAlpha);
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
	QScopedValueRollback<bool> rollback(m_updating, true);
	m_item = {
		0,	   QString(), QColor(), 1.0f,  DP_BLEND_MODE_NORMAL,
		0.0f,  QColor(),  false,	false, false,
		group, false,	  false,	group, 0,
		0,	   0,		  0,
	};
	m_selectedId = selectedId;
	m_wasDefault = false;
	setWindowTitle(group ? tr("New Layer Group") : tr("New Layer"));
	m_ui->buttonBox->buttons()[0]->setChecked(true);
	m_ui->title->setText(title);
	m_ui->opacitySlider->setValue(100);
	m_ui->alphaBlend->setChecked(true);
	m_ui->visible->setChecked(true);
	m_ui->censored->setChecked(false);
	setSketchParamsFromSettings();
	m_ui->defaultLayer->setChecked(false);
	m_ui->createdBy->setToolTip(QString());
	m_ui->createdByLabel->hide();
	m_ui->createdBy->hide();
	updateBlendMode(
		m_ui->blendMode, m_item.blend, m_item.group, m_item.isolated,
		m_item.clip, m_automaticAlphaPreserve);
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
	QScopedValueRollback<bool> rollback(m_updating, true);
	m_item = item;
	m_wasDefault = isDefault;

	m_ui->title->setText(item.title);
	int rgb = item.color.isValid() ? int(item.color.rgb() & 0xffffffu) : 0;
	QAbstractButton *colorButton = m_colorButtons->button(rgb);
	if(colorButton) {
		colorButton->setChecked(true);
	} else {
		m_colorButtons->buttons()[0]->setChecked(true);
	}
	m_ui->opacitySlider->setValue(qRound(item.opacity * 100.0f));
	if(item.clip) {
		m_ui->clip->setChecked(true);
	} else if(canvas::blendmode::presentsAsAlphaPreserving(item.blend)) {
		m_ui->alphaPreserve->setChecked(true);
	} else {
		m_ui->alphaBlend->setChecked(true);
	}
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
	m_ui->createdBy->setToolTip(
		QStringLiteral("Layer id 0x%1, context id %2, element id %3")
			.arg(
				QString::number(item.id, 16), QString::number(item.creatorId()),
				QString::number(item.elementId())));
	updateBlendMode(
		m_ui->blendMode, item.blend, item.group, item.isolated, item.clip,
		m_automaticAlphaPreserve);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindAutomaticAlphaPreserve(
		this, &LayerProperties::setAutomaticAlphaPerserve);
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

void LayerProperties::updateBlendMode(
	QComboBox *combo, DP_BlendMode mode, bool group, bool isolated, bool clip,
	bool automaticAlphaPreserve)
{
	combo->setModel(getLayerBlendModesFor(group, clip, automaticAlphaPreserve));
	// If this is an unknown blend mode, hide the control to avoid damage.
	int blendModeIndex = searchBlendModeComboIndex(combo, int(mode));
	combo->setVisible(blendModeIndex != -1);
	combo->setCurrentIndex(group && !isolated ? 0 : blendModeIndex);
}

void LayerProperties::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	m_ui->title->setFocus(Qt::PopupFocusReason);
	m_ui->title->selectAll();
}

void LayerProperties::setAutomaticAlphaPerserve(bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve != m_automaticAlphaPreserve) {
		m_automaticAlphaPreserve = automaticAlphaPreserve;
		updateAlpha(m_ui->alphaGroup->checkedButton());
	}
}

void LayerProperties::updateAlpha(QAbstractButton *button)
{
	if(!m_updating) {
		QScopedValueRollback<bool> rollback(m_updating, true);
		bool clip = button == m_ui->clip;
		bool alphaPreserve = button == m_ui->alphaPreserve;
		int blendMode = m_ui->blendMode->currentData().toInt();
		m_ui->blendMode->setModel(getLayerBlendModesFor(
			m_item.group, clip, m_automaticAlphaPreserve));

		if((clip || !alphaPreserve) && blendMode == DP_BLEND_MODE_RECOLOR) {
			blendMode = DP_BLEND_MODE_NORMAL;
		} else if(!clip && alphaPreserve && blendMode == DP_BLEND_MODE_NORMAL) {
			blendMode = DP_BLEND_MODE_RECOLOR;
		}

		int index = searchBlendModeComboIndex(m_ui->blendMode, blendMode);
		m_ui->blendMode->setCurrentIndex(index == -1 ? 0 : index);
	}
}

void LayerProperties::updateAlphaBasedOnBlendMode(int index)
{
	if(!m_updating && m_automaticAlphaPreserve && !m_ui->clip->isChecked()) {
		QScopedValueRollback<bool> rollback(m_updating, true);
		int blendMode = m_ui->blendMode->itemData(index).toInt();
		if(canvas::blendmode::presentsAsAlphaPreserving(blendMode)) {
			m_ui->alphaPreserve->setChecked(true);
		} else {
			m_ui->alphaBlend->setChecked(true);
		}
	}
}

void LayerProperties::updateAlphaTogglesBasedOnBlendMode(int index)
{
	int blendMode = m_ui->blendMode->itemData(index).toInt();
	bool enabled = blendMode != -1;
	m_ui->alphaBlend->setEnabled(enabled);
	m_ui->alphaPreserve->setEnabled(enabled);
	m_ui->clip->setEnabled(enabled);
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
		int blendMode = m_ui->blendMode->currentData().toInt();
		if(blendMode == -1) {
			blendMode = DP_BLEND_MODE_NORMAL;
		}

		bool clip = m_ui->clip->isChecked();
		bool alphaPreserve = m_ui->alphaPreserve->isChecked();
		if(clip) {
			blendMode = canvas::blendmode::toAlphaAffecting(blendMode);
		} else if(alphaPreserve) {
			blendMode = canvas::blendmode::toAlphaPreserving(blendMode);
		}

		int sketchOpacity = m_ui->sketchMode->isChecked()
								? m_ui->sketchOpacitySlider->value()
								: 0;
		QColor sketchTint =
			sketchOpacity <= 0 ? QColor() : m_ui->sketchTintPreview->color();
		saveSketchParametersToSettings(sketchOpacity, sketchTint);
		emit addLayerOrGroupRequested(
			m_selectedId, m_item.group, getTitleWithColor(),
			m_ui->opacitySlider->value(), blendMode,
			m_item.group && blendMode != -1, m_ui->censored->isChecked(), clip,
			m_ui->defaultLayer->isChecked(), m_ui->visible->isChecked(),
			sketchOpacity, sketchTint);
	} else {
		emitChanges();
	}
}

QString LayerProperties::getTitleWithColor() const
{
	int checkedColorId = m_colorButtons->checkedId();
	QColor color =
		checkedColorId <= 0 ? QColor() : QColor::fromRgb(QRgb(checkedColorId));
	return canvas::LayerListItem::makeTitleWithColor(
		m_ui->title->text(), color);
}

void LayerProperties::emitChanges()
{
	net::MessageList messages;

	QString title = getTitleWithColor();
	if(m_item.titleWithColor() != title) {
		messages.append(net::makeLayerRetitleMessage(m_user, m_item.id, title));
	}

	int oldOpacity = qRound(m_item.opacity * 100.0);
	bool censored = m_ui->censored->isChecked();
	bool clip = m_ui->clip->isChecked();
	bool alphaPreserve = m_ui->alphaPreserve->isChecked();
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

	if(clip) {
		newBlendmode = DP_BlendMode(
			canvas::blendmode::toAlphaAffecting(int(newBlendmode)));
	} else if(alphaPreserve) {
		newBlendmode = DP_BlendMode(
			canvas::blendmode::toAlphaPreserving(int(newBlendmode)));
	}

	if(m_ui->opacitySlider->value() != oldOpacity ||
	   newBlendmode != m_item.blend || censored != m_item.actuallyCensored() ||
	   isolated != m_item.isolated || clip != m_item.clip) {
		uint8_t flags =
			(censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0) |
			(isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED : 0) |
			(clip ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CLIP : 0);
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

}
