/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "layerproperties.h"
#include "ui_layerproperties.h"

#include "core/blendmodes.h"
#include "canvas/layerlist.h"

namespace dialogs {

LayerProperties::LayerProperties(QWidget *parent)
    : QDialog(parent)
{
    m_ui = new Ui_LayerProperties;
    m_ui->setupUi(this);

	for(auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::LayerMode)) {
		m_ui->blendMode->addItem(bm.second, bm.first);
    }

    connect(m_ui->title, &QLineEdit::returnPressed, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &LayerProperties::emitChanges);
}

void LayerProperties::setLayerData(const LayerData &data)
{
    m_layerData = data;
    applyLayerDataToUi();
}

void LayerProperties::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_ui->title->setFocus(Qt::PopupFocusReason);
    m_ui->title->selectAll();
}

void LayerProperties::emitChanges()
{
    ChangedLayerData c;
    c.id = m_layerData.id;
    c.changes = CHANGE_NOTHING;

    c.title = m_ui->title->text();
    if(c.title != m_layerData.title) {
        c.changes |= CHANGE_TITLE;
    }

    int opacity = m_ui->opacitySpinner->value();
    if(opacity != layerDataOpacity()) {
        c.opacity = opacity / 100.0f;
        c.changes |= CHANGE_OPACITY;
    }

    if(m_ui->blendMode->isEnabled()) {
        c.blend = static_cast<paintcore::BlendMode::Mode>(
                m_ui->blendMode->currentData().toInt());
        if(c.blend != m_layerData.blend) {
            c.changes |= CHANGE_BLEND;
        }
    }

    c.hidden = !m_ui->visible->isChecked();
    if(c.hidden != m_layerData.hidden) {
        c.changes |= CHANGE_HIDDEN;
    }

    c.fixed = m_ui->fixed->isChecked();
    if(c.fixed != m_layerData.fixed) {
        c.changes |= CHANGE_FIXED;
    }

    if(m_ui->defaultLayer->isEnabled() && m_ui->defaultLayer->isChecked()) {
        c.changes |= CHANGE_DEFAULT;
        c.defaultLayer = true;
    }

    if(c.changes != CHANGE_NOTHING) {
        emit propertiesChanged(c);
    }
}

void LayerProperties::applyLayerDataToUi()
{
    m_ui->title->setText(m_layerData.title);
    m_ui->opacitySpinner->setValue(layerDataOpacity());
    m_ui->visible->setChecked(!m_layerData.hidden);
    m_ui->fixed->setChecked(m_layerData.fixed);
    m_ui->defaultLayer->setChecked(m_layerData.defaultLayer);
    m_ui->defaultLayer->setEnabled(!m_layerData.defaultLayer);
    int blendModeIndex = searchBlendModeIndex(m_layerData.blend);
    if(blendModeIndex == -1) {
        // Apparently we don't know this blend mode, probably because
        // this client is outdated. Disable the control to avoid damage.
        m_ui->blendMode->setCurrentIndex(-1);
        m_ui->blendMode->setEnabled(false);
    } else {
        m_ui->blendMode->setCurrentIndex(blendModeIndex);
        m_ui->blendMode->setEnabled(true);
    }
}

int LayerProperties::layerDataOpacity()
{
    return qRound(m_layerData.opacity * 100.0f);
}

int LayerProperties::searchBlendModeIndex(paintcore::BlendMode::Mode mode)
{
    int blendModeCount = m_ui->blendMode->count();
    for(int i = 0; i < blendModeCount; ++i) {
        if(m_ui->blendMode->itemData(i).toInt() == mode) {
            return i;
        }
    }
    return -1;
}

}
