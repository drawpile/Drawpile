// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/toolbarconfigdialog.h"
#include "desktop/utils/widgetutils.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

using std::placeholders::_1;
using std::placeholders::_2;

namespace dialogs {

ToolBarConfigDialog::ToolBarConfigDialog(
	const QVariantHash &cfg, const QList<QAction *> &drawActions,
	QWidget *parent)
	: QDialog(parent)
	, m_drawActions(drawActions)
{
	setWindowTitle(tr("Configure Toolbar"));
	setWindowModality(Qt::WindowModal);
	resize(400, 600);

	QVBoxLayout *layout = new QVBoxLayout(this);

	QLabel *label =
		new QLabel(tr("Drag to reorder and uncheck to hide tools."));
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(true);
	layout->addWidget(label);

	m_drawList = new QListWidget;
	m_drawList->setDragDropMode(QAbstractItemView::InternalMove);
	m_drawList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_drawList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_drawList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	utils::bindKineticScrolling(m_drawList);
	layout->addWidget(m_drawList, 1);

	QDialogButtonBox *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
		QDialogButtonBox::Reset);
	layout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ToolBarConfigDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ToolBarConfigDialog::reject);
	connect(
		buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
		&ToolBarConfigDialog::resetDrawActions);

	loadDrawActions(cfg);
}

void ToolBarConfigDialog::updateConfig(QVariantHash &cfg) const
{
	QVariantList drawCfgActions;
	int count = m_drawList->count();
	drawCfgActions.reserve(count);
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_drawList->item(i);
		QVariantHash drawCfgAction;
		drawCfgAction.insert(
			QStringLiteral("name"), item->data(Qt::UserRole).toString());
		if(item->checkState() == Qt::Unchecked) {
			drawCfgAction.insert(QStringLiteral("hidden"), true);
		}
		drawCfgActions.append(drawCfgAction);
	}
	cfg.insert(
		QStringLiteral("draw"),
		QVariantHash({{QStringLiteral("actions"), drawCfgActions}}));
}

void ToolBarConfigDialog::readConfig(
	const QVariantHash &cfg, const QString &toolBarName,
	QList<QAction *> actions, const ReadConfigFn &fn)
{
	QVariantList drawCfgActions = cfg.value(toolBarName)
									  .toHash()
									  .value(QStringLiteral("actions"))
									  .toList();

	for(const QVariant &v : drawCfgActions) {
		QHash actionCfg = v.toHash();
		QString name = actionCfg.value("name").toString();
		for(int i = 0, count = actions.size(); i < count; ++i) {
			QAction *action = actions[i];
			if(action->objectName() == name) {
				fn(action, actionCfg.value("hidden").toBool());
				actions.removeAt(i);
				break;
			}
		}
	}

	for(QAction *action : actions) {
		fn(action, false);
	}
}

void ToolBarConfigDialog::loadDrawActions(const QVariantHash &cfg)
{
	m_drawList->clear();
	readConfig(
		cfg, QStringLiteral("draw"), m_drawActions,
		std::bind(&ToolBarConfigDialog::addDrawAction, this, _1, _2));
}

void ToolBarConfigDialog::resetDrawActions()
{
	QMessageBox *box = utils::showQuestion(
		this, tr("Reset"),
		tr("Really revert the toolbar configuration to its default state?"));
	connect(
		box, &QMessageBox::accepted, this,
		std::bind(&ToolBarConfigDialog::loadDrawActions, this, QVariantHash()));
}

void ToolBarConfigDialog::addDrawAction(QAction *action, bool hidden)
{
	QListWidgetItem *item = new QListWidgetItem;
	item->setIcon(action->icon());
	item->setText(utils::scrubAccelerators(action->text()));
	item->setData(Qt::UserRole, action->objectName());
	item->setFlags(
		Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
		Qt::ItemIsUserCheckable);
	item->setCheckState(hidden ? Qt::Unchecked : Qt::Checked);
	m_drawList->addItem(item);
}

}
