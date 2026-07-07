// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/brushsavedialog.h"
#include "desktop/dialogs/brushsettingsdialog.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/brushes/brushpresetmodel.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace dialogs {

BrushSaveDialog::BrushSaveDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Save Brush"));
	utils::makeModal(this);
	resize(400, 500);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_presetForm = new BrushPresetForm(false, false);
	m_presetForm->setContentsMargins(0, 0, 0, 0);
	dlgLayout->addWidget(m_presetForm);

	QFormLayout *form = m_presetForm->form();
	form->setContentsMargins(0, 0, 0, 0);

	m_tagList = new QListWidget;
	m_tagList->setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	form->addRow(tr("Tags:"), m_tagList);

	QDialogButtonBox *buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this, &BrushSaveDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this, &BrushSaveDialog::reject);
}

void BrushSaveDialog::setPreset(const brushes::Preset &preset)
{
	m_presetForm->setPresetName(preset.effectiveName());
	m_presetForm->setPresetDescription(preset.effectiveDescription());
	m_presetForm->setPresetThumbnail(preset.effectiveThumbnailPixmap());
	m_presetForm->setTakeable(!preset.effectiveBrushLoad().isConfidential());
}

void BrushSaveDialog::addTag(int id, const QString &name)
{
	if(id > 0) {
		QListWidgetItem *item = new QListWidgetItem(name);
		item->setData(Qt::UserRole, id);
		item->setFlags(
			Qt::ItemIsUserCheckable | Qt::ItemIsEnabled |
			Qt::ItemNeverHasChildren);
		item->setCheckState(Qt::Unchecked);
		m_tagList->addItem(item);
	}
}

QString BrushSaveDialog::presetName() const
{
	return m_presetForm->presetName();
}

QString BrushSaveDialog::presetDescription() const
{
	return m_presetForm->presetDescription();
}

QPixmap BrushSaveDialog::presetThumbnail() const
{
	return m_presetForm->presetThumbnail();
}

QSet<int> BrushSaveDialog::presetTagIds() const
{
	QSet<int> tagIds;
	int count = m_tagList->count();
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_tagList->item(i);
		if(item && item->checkState() != Qt::Unchecked) {
			tagIds.insert(item->data(Qt::UserRole).toInt());
		}
	}
	return tagIds;
}

}
