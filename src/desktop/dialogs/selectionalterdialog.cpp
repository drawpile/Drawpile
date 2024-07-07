// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/selectionalterdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace dialogs {

SelectionAlterDialog::SelectionAlterDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowModality(Qt::WindowModal);
	QVBoxLayout *layout = new QVBoxLayout(this);

	QHBoxLayout *expandLayout = new QHBoxLayout;
	expandLayout->setSpacing(0);

	m_expandSpinner = new QSpinBox;
	m_expandSpinner->setRange(0, 9999);
	m_expandSpinner->setValue(getIntProperty(parent, PROP_EXPAND));
	m_expandSpinner->setSuffix(tr("px"));
	expandLayout->addWidget(m_expandSpinner, 1);
	connect(
		m_expandSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&SelectionAlterDialog::updateControls);

	expandLayout->addSpacing(3);

	widgets::GroupedToolButton *expandButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	expandButton->setIcon(QIcon::fromTheme("zoom-in"));
	expandButton->setStatusTip(tr("Expand"));
	expandButton->setToolTip(expandButton->statusTip());
	expandButton->setCheckable(true);
	expandButton->setChecked(!getBoolProperty(parent, PROP_SHRINK));
	expandLayout->addWidget(expandButton);

	widgets::GroupedToolButton *shrinkButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	shrinkButton->setIcon(QIcon::fromTheme("zoom-out"));
	shrinkButton->setStatusTip(tr("Shrink"));
	shrinkButton->setToolTip(shrinkButton->statusTip());
	shrinkButton->setCheckable(true);
	shrinkButton->setChecked(!expandButton->isChecked());
	expandLayout->addWidget(shrinkButton);

	m_expandGroup = new QButtonGroup(this);
	m_expandGroup->addButton(expandButton, 0);
	m_expandGroup->addButton(shrinkButton, 1);
	connect(
		m_expandGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&SelectionAlterDialog::updateControls);

	layout->addLayout(expandLayout);

	m_featherSpinner = new QSpinBox;
	m_featherSpinner->setRange(0, 999);
	m_featherSpinner->setValue(getIntProperty(parent, PROP_FEATHER));
	//: "Feather" is a verb here, referring to blurring the selection.
	m_featherSpinner->setPrefix(tr("Feather: "));
	m_featherSpinner->setSuffix(tr("px"));
	layout->addWidget(m_featherSpinner);
	connect(
		m_featherSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&SelectionAlterDialog::updateControls);

	m_fromEdgeBox = new QCheckBox;
	m_fromEdgeBox->setChecked(getBoolProperty(parent, PROP_FROM_EDGE));
	layout->addWidget(m_fromEdgeBox);

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(
		m_buttons, &QDialogButtonBox::accepted, this,
		&SelectionAlterDialog::accept);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this,
		&SelectionAlterDialog::reject);
	layout->addWidget(m_buttons);

	connect(
		this, &SelectionAlterDialog::accepted, this,
		&SelectionAlterDialog::emitAlterSelectionRequested);

	updateControls();
	resize(300, 150);
}

bool SelectionAlterDialog::getBoolProperty(QWidget *w, const char *name)
{
	return w && w->property(name).toBool();
}

int SelectionAlterDialog::getIntProperty(QWidget *w, const char *name)
{
	return w ? w->property(name).toInt() : 0;
}

void SelectionAlterDialog::updateControls()
{
	bool shrink = m_expandGroup->checkedId() != 0;
	bool haveExpand = m_expandSpinner->value() != 0;
	bool haveFeather = m_featherSpinner->value() != 0;

	if(shrink) {
		m_expandSpinner->setPrefix(tr("Shrink: "));
		//: "Feather" is a verb here, referring to blurring the selection.
		m_fromEdgeBox->setText(tr("Shrink and feather from canvas edge"));
	} else {
		m_expandSpinner->setPrefix(tr("Expand: "));
		//: "Feather" is a verb here, referring to blurring the selection.
		m_fromEdgeBox->setText(tr("Feather from canvas edge"));
	}

	QPushButton *okButton = m_buttons->button(QDialogButtonBox::Ok);
	if(okButton) {
		okButton->setEnabled(haveExpand || haveFeather);
	}
}

void SelectionAlterDialog::emitAlterSelectionRequested()
{
	bool shrink = m_expandGroup->checkedId() != 0;
	int expand = m_expandSpinner->value();
	int feather = m_featherSpinner->value();
	bool fromEdge = m_fromEdgeBox->isChecked();
	QWidget *pw = parentWidget();
	if(pw) {
		pw->setProperty(PROP_SHRINK, shrink);
		pw->setProperty(PROP_EXPAND, expand);
		pw->setProperty(PROP_FEATHER, feather);
		pw->setProperty(PROP_FROM_EDGE, fromEdge);
	}
	emit alterSelectionRequested(expand * (shrink ? -1 : 1), feather, fromEdge);
}

}
