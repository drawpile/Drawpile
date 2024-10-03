// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/flood_fill.h>
}
#include "desktop/dialogs/selectionalterdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/expandshrinkspinner.h"
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

	m_expandShrinkSpinner = new widgets::ExpandShrinkSpinner(false);
	m_expandShrinkSpinner->setSpinnerRange(0, 9999);
	m_expandShrinkSpinner->setSpinnerValue(getIntProperty(parent, PROP_EXPAND));
	m_expandShrinkSpinner->setShrink(getBoolProperty(parent, PROP_SHRINK));
	m_expandShrinkSpinner->setKernel(getIntProperty(parent, PROP_KERNEL));
	layout->addWidget(m_expandShrinkSpinner);
	connect(
		m_expandShrinkSpinner,
		&widgets::ExpandShrinkSpinner::spinnerValueChanged, this,
		&SelectionAlterDialog::updateControls);

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
	bool shrink = m_expandShrinkSpinner->isShrink();
	bool haveExpand = m_expandShrinkSpinner->spinnerValue() != 0;
	bool haveFeather = m_featherSpinner->value() != 0;

	if(shrink) {
		//: "Feather" is a verb here, referring to blurring the selection.
		m_fromEdgeBox->setText(tr("Shrink and feather from canvas edge"));
	} else {
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
	bool shrink = m_expandShrinkSpinner->isShrink();
	int expand = m_expandShrinkSpinner->spinnerValue();
	int kernel = m_expandShrinkSpinner->kernel();
	int feather = m_featherSpinner->value();
	bool fromEdge = m_fromEdgeBox->isChecked();
	QWidget *pw = parentWidget();
	if(pw) {
		pw->setProperty(PROP_SHRINK, shrink);
		pw->setProperty(PROP_EXPAND, expand);
		pw->setProperty(PROP_KERNEL, kernel);
		pw->setProperty(PROP_FEATHER, feather);
		pw->setProperty(PROP_FROM_EDGE, fromEdge);
	}
	emit alterSelectionRequested(
		m_expandShrinkSpinner->effectiveValue(), kernel, feather, fromEdge);
}

}
