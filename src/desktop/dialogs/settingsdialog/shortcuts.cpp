// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/proportionaltableview.h"
#include "desktop/settings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/keysequenceedit.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libshared/util/qtcompat.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <functional>

namespace dialogs {
namespace settingsdialog {

Shortcuts::Shortcuts(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	auto *filter = new QLineEdit;
	filter->setClearButtonEnabled(true);
	filter->setPlaceholderText(tr("Search actions…"));
	filter->addAction(
		QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	initGlobalShortcuts(settings, layout, filter);
}

class KeySequenceEditFactory final : public QItemEditorCreatorBase {
public:
	QWidget *createWidget(QWidget *parent) const override
	{
		return new widgets::KeySequenceEdit(parent);
	}

	QByteArray valuePropertyName() const override { return "keySequence"; }
};

void Shortcuts::initGlobalShortcuts(
	desktop::settings::Settings &settings, QVBoxLayout *layout,
	QLineEdit *filter)
{
	layout->addWidget(filter);

	auto *shortcutsModel = new CustomShortcutModel(this);
	// This is also a gross way to access this model, but since these models are
	// not first-class models and I am not rewriting them right now, this is how
	// it will be in order to get the “Restore defaults” hooked up to everything
	m_globalShortcutsModel = shortcutsModel;
	shortcutsModel->loadShortcuts(settings.shortcuts());
	connect(
		shortcutsModel, &QAbstractItemModel::dataChanged, &settings,
		[=, &settings] {
			settings.setShortcuts(shortcutsModel->saveShortcuts());
		});
	auto *shortcuts = ProportionalTableView::make(filter, shortcutsModel);
	shortcuts->setColumnStretches({6, 2, 2, 2});

	QStyledItemDelegate *keySequenceDelegate = new QStyledItemDelegate(this);
	m_itemEditorFactory.registerEditor(
		compat::metaTypeFromName("QKeySequence"), new KeySequenceEditFactory);
	keySequenceDelegate->setItemEditorFactory(&m_itemEditorFactory);
	shortcuts->setItemDelegateForColumn(1, keySequenceDelegate);
	shortcuts->setItemDelegateForColumn(2, keySequenceDelegate);
	// The key sequence editor only saves when the index changes, but closing
	// the window doesn't change the index and so causes the seemingly changed
	// shortcut to not apply. So we use some additional force here.
	connect(this, &QObject::destroyed, this, [=] {
		shortcuts->setCurrentIndex(QModelIndex{});
	});

	layout->addWidget(shortcuts, 1);

	auto *actions = new utils::EncapsulatedLayout;
	actions->setContentsMargins(0, 0, 0, 0);
	actions->setSpacing(0);
	actions->addStretch();

	auto *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [=, &settings] {
		const auto confirm = execConfirm(
			tr("Restore Shortcut Defaults"),
			tr("Really restore all shortcuts to their default values?"), this);
		if(confirm) {
			settings.setShortcuts({});
			m_globalShortcutsModel->loadShortcuts({});
		}
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);
}

} // namespace settingsdialog
} // namespace dialogs
