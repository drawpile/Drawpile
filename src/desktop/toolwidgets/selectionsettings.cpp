// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> antialias{
	QStringLiteral("antialias"), false};
}

SelectionSettings::SelectionSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
	connect(
		ctrl, &ToolController::modelChanged, this,
		&SelectionSettings::setModel);
}

ToolProperties SelectionSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::antialias, m_antiAliasCheckBox->isChecked());
	return cfg;
}

void SelectionSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_antiAliasCheckBox->setChecked(cfg.value(props::antialias));
}

bool SelectionSettings::isLocked()
{
	// If we're connected to a thick server, we don't want to send it unknown
	// message types because that'll get us kicked, which requires us to
	// disguise selection commands as PutImage messages. Those in turn require
	// the appropriate permission.
	return !m_putImageAllowed &&
		   controller()->client()->seemsConnectedToThickServer();
}

void SelectionSettings::setAction(QAction *starttransform)
{
	Q_ASSERT(m_startTransformButton);
	m_startTransformButton->setIcon(starttransform->icon());
	m_startTransformButton->setText(starttransform->text());
	m_startTransformButton->setStatusTip(starttransform->statusTip());
	m_startTransformButton->setToolTip(starttransform->statusTip());
	connect(
		m_startTransformButton, &QPushButton::clicked, starttransform,
		&QAction::trigger);
}

void SelectionSettings::pushSettings()
{
	ToolController::SelectionParams selectionParams;
	selectionParams.antiAlias = m_antiAliasCheckBox->isChecked();
	selectionParams.defaultOp = m_headerGroup->checkedId();
	controller()->setSelectionParams(selectionParams);
}

QWidget *SelectionSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	m_headerGroup = new QButtonGroup(m_headerWidget);

	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);
	headerLayout->addStretch();

	widgets::GroupedToolButton *replaceButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	replaceButton->setIcon(QIcon::fromTheme("drawpile_selection_replace"));
	replaceButton->setStatusTip(tr("Replace selection"));
	replaceButton->setToolTip(replaceButton->statusTip());
	replaceButton->setCheckable(true);
	replaceButton->setChecked(true);
	headerLayout->addWidget(replaceButton);
	m_headerGroup->addButton(replaceButton, DP_MSG_SELECTION_PUT_OP_REPLACE);

	widgets::GroupedToolButton *uniteButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	uniteButton->setIcon(QIcon::fromTheme("drawpile_selection_unite"));
	uniteButton->setStatusTip(tr("Add to selection"));
	uniteButton->setToolTip(uniteButton->statusTip());
	uniteButton->setCheckable(true);
	headerLayout->addWidget(uniteButton);
	m_headerGroup->addButton(uniteButton, DP_MSG_SELECTION_PUT_OP_UNITE);

	widgets::GroupedToolButton *intersectButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	intersectButton->setIcon(QIcon::fromTheme("drawpile_selection_intersect"));
	intersectButton->setStatusTip(tr("Intersect with selection"));
	intersectButton->setToolTip(intersectButton->statusTip());
	intersectButton->setCheckable(true);
	headerLayout->addWidget(intersectButton);
	m_headerGroup->addButton(
		intersectButton, DP_MSG_SELECTION_PUT_OP_INTERSECT);

	widgets::GroupedToolButton *m_excludeButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_excludeButton->setIcon(QIcon::fromTheme("drawpile_selection_exclude"));
	m_excludeButton->setStatusTip(tr("Remove from selection"));
	m_excludeButton->setToolTip(m_excludeButton->statusTip());
	m_excludeButton->setCheckable(true);
	headerLayout->addWidget(m_excludeButton);
	m_headerGroup->addButton(m_excludeButton, DP_MSG_SELECTION_PUT_OP_EXCLUDE);

	headerLayout->addStretch();

	connect(
		m_headerGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&SelectionSettings::pushSettings);

	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);

	m_antiAliasCheckBox = new QCheckBox(tr("Anti-aliasing"));
	m_antiAliasCheckBox->setStatusTip(tr("Smoothe out selection edges"));
	m_antiAliasCheckBox->setToolTip(m_antiAliasCheckBox->statusTip());
	connect(
		m_antiAliasCheckBox, &QCheckBox::clicked, this,
		&SelectionSettings::pushSettings);
	layout->addWidget(m_antiAliasCheckBox);

	m_startTransformButton = new QPushButton;
	m_startTransformButton->setEnabled(false);
	layout->addWidget(m_startTransformButton);

	layout->addStretch();

	return widget;
}

void SelectionSettings::setModel(canvas::CanvasModel *canvas)
{
	if(canvas) {
		connect(
			canvas->selection(), &canvas::SelectionModel::selectionChanged,
			this, &SelectionSettings::updateEnabled);
	}
	updateEnabledFrom(canvas);
}

void SelectionSettings::updateEnabled()
{
	updateEnabledFrom(controller()->model());
}

void SelectionSettings::updateEnabledFrom(canvas::CanvasModel *canvas)
{
	if(m_startTransformButton) {
		bool haveSelection = canvas && canvas->selection()->isValid();
		m_startTransformButton->setEnabled(haveSelection);
	}
}

}
