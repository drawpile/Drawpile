// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/layerproperties.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/drawdance/message.h"

#include "ui_layerproperties.h"


namespace dialogs {

LayerProperties::LayerProperties(uint8_t localUser, QWidget *parent)
	: QDialog(parent), m_user(localUser)
{
    m_ui = new Ui_LayerProperties;
    m_ui->setupUi(this);

	const auto modes = canvas::blendmode::layerModeNames();
	for(const auto &m : modes) {
		m_ui->blendMode->addItem(m.name, int(m.mode));
    }

    connect(m_ui->title, &QLineEdit::returnPressed, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_ui->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *b) {
		if(m_ui->buttonBox->buttonRole(b) == QDialogButtonBox::ApplyRole)
			emitChanges();
	});
	connect(this, &QDialog::accepted, this, &LayerProperties::emitChanges);
}

LayerProperties::~LayerProperties()
{
	delete m_ui;
}

void LayerProperties::setLayerItem(const canvas::LayerListItem &item, const QString &creator, bool isDefault)
{
	m_item = item;
	m_wasDefault = isDefault;

	m_ui->title->setText(item.title);
	m_ui->opacitySlider->setValue(qRound(item.opacity * 100.0f));
	m_ui->visible->setChecked(!item.hidden);
	m_ui->censored->setChecked(item.censored);
	m_ui->defaultLayer->setChecked(isDefault);

	int blendModeIndex = searchBlendModeIndex(item.blend);
	if(blendModeIndex == -1) {
		// Apparently we don't know this blend mode, probably because
		// this client is outdated. Disable the control to avoid damage.
		m_ui->blendMode->setCurrentIndex(-1);
		m_ui->blendMode->setEnabled(false);

	} else {
		m_ui->blendMode->setCurrentIndex(blendModeIndex);
		m_ui->blendMode->setEnabled(true);
	}

	m_ui->passThrough->setChecked(!item.group || !item.isolated);
	m_ui->passThrough->setVisible(item.group);
	m_ui->createdBy->setText(creator);
}

void LayerProperties::setControlsEnabled(bool enabled) {
	QWidget *w[] = {
		m_ui->title,
		m_ui->opacitySlider,
		m_ui->blendMode,
		m_ui->passThrough,
		m_ui->censored
	};
	for(unsigned int i=0;i<sizeof(w)/sizeof(*w);++i)
		w[i]->setEnabled(enabled);
}

void LayerProperties::setOpControlsEnabled(bool enabled)
{
	m_ui->defaultLayer->setEnabled(enabled);
}

void LayerProperties::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_ui->title->setFocus(Qt::PopupFocusReason);
    m_ui->title->selectAll();
}

void LayerProperties::emitChanges()
{
	drawdance::MessageList messages;

	QString title = m_ui->title->text();
	if(m_item.title != title) {
		messages.append(drawdance::Message::makeLayerRetitle(m_user, m_item.id, title));
	}

	const int oldOpacity = qRound(m_item.opacity * 100.0);
	const DP_BlendMode newBlendmode = DP_BlendMode(m_ui->blendMode->currentData().toInt());
	const bool censored = m_ui->censored->isChecked();
	const bool isolated = !m_ui->passThrough->isChecked();

	if(
		m_ui->opacitySlider->value() != oldOpacity ||
		newBlendmode != m_item.blend ||
		censored != m_item.censored ||
	    isolated != m_item.isolated
	) {
		uint8_t flags =
			(censored ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR : 0) |
			(isolated ? DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED : 0);
		uint8_t opacity = qRound(m_ui->opacitySlider->value() / 100.0 * 255);
		messages.append(drawdance::Message::makeLayerAttributes(
			m_user, m_item.id, 0, flags, opacity, newBlendmode));
    }

	if(m_ui->visible->isChecked() != (!m_item.hidden)) {
		emit visibilityChanged(m_item.id, m_ui->visible->isChecked());
    }

	bool makeDefault = m_ui->defaultLayer->isChecked();
    if(m_ui->defaultLayer->isEnabled() && makeDefault != m_wasDefault) {
		messages.append(drawdance::Message::makeDefaultLayer(
			m_user, makeDefault ? m_item.id : 0));
    }

	emit layerCommands(messages.count(), messages.constData());
}

int LayerProperties::searchBlendModeIndex(DP_BlendMode mode)
{
	const int blendModeCount = m_ui->blendMode->count();
    for(int i = 0; i < blendModeCount; ++i) {
		if(m_ui->blendMode->itemData(i).toInt() == int(mode)) {
            return i;
        }
    }
    return -1;
}

}
