// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/canvasshortcutsdialog.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/proportionaltableview.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/keysequenceedit.h"
#include "libclient/utils/canvasshortcutsmodel.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libshared/util/qtcompat.h"
#include <QAbstractItemView>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace dialogs {
namespace settingsdialog {

Shortcuts::Shortcuts(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	QLineEdit *filter = new QLineEdit;
	filter->setClearButtonEnabled(true);
	filter->setPlaceholderText(tr("Search actions…"));
	filter->addAction(
		QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	layout->addWidget(filter);

	m_tabs = new QTabWidget;
	m_tabs->addTab(
		initActionShortcuts(settings, filter),
		QIcon::fromTheme("input-keyboard"), tr("Actions"));
	m_tabs->addTab(
		initCanvasShortcuts(settings, filter), QIcon::fromTheme("edit-image"),
		tr("Canvas"));
	m_tabs->addTab(
		initTouchShortcuts(settings), QIcon::fromTheme("hand"), tr("Touch"));
	layout->addWidget(m_tabs);
}

void Shortcuts::finishEditing()
{
	m_shortcutsTable->setCurrentIndex(QModelIndex{});
}

class KeySequenceEditFactory final : public QItemEditorCreatorBase {
public:
	KeySequenceEditFactory(Shortcuts *shortcuts)
		: m_shortcuts{shortcuts}
	{
	}

	QWidget *createWidget(QWidget *parent) const override
	{
		widgets::KeySequenceEdit *widget = new widgets::KeySequenceEdit(parent);
		QObject::connect(
			widget, &widgets::KeySequenceEdit::editingFinished, m_shortcuts,
			&Shortcuts::finishEditing);
		return widget;
	}

	QByteArray valuePropertyName() const override
	{
		return QByteArrayLiteral("keySequence");
	}

private:
	Shortcuts *m_shortcuts;
};

QWidget *Shortcuts::initActionShortcuts(
	desktop::settings::Settings &settings, QLineEdit *filter)
{
	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	CustomShortcutModel *shortcutsModel = new CustomShortcutModel(this);
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
	connect(
		&dpApp(), &DrawpileApp::shortcutsChanged, shortcutsModel,
		&CustomShortcutModel::updateShortcuts);
	m_shortcutsTable = ProportionalTableView::make(filter, shortcutsModel);
	m_shortcutsTable->setColumnStretches({6, 2, 2, 2});
	utils::bindKineticScrolling(m_shortcutsTable);

	QStyledItemDelegate *keySequenceDelegate = new QStyledItemDelegate(this);
	m_itemEditorFactory.registerEditor(
		compat::metaTypeFromName("QKeySequence"),
		new KeySequenceEditFactory{this});
	keySequenceDelegate->setItemEditorFactory(&m_itemEditorFactory);
	m_shortcutsTable->setItemDelegateForColumn(1, keySequenceDelegate);
	m_shortcutsTable->setItemDelegateForColumn(2, keySequenceDelegate);

	layout->addWidget(m_shortcutsTable, 1);

	utils::EncapsulatedLayout *actions = new utils::EncapsulatedLayout;
	actions->setContentsMargins(0, 0, 0, 0);
	actions->setSpacing(0);
	actions->addStretch();

	QPushButton *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [=, &settings] {
		bool confirm = execConfirm(
			tr("Restore Shortcut Defaults"),
			tr("Really restore all shortcuts to their default values?"), this);
		if(confirm) {
			settings.setShortcuts({});
			m_globalShortcutsModel->loadShortcuts({});
		}
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);

	return widget;
}

template <typename Fn>
static void
onAnyModelChange(QAbstractItemModel *model, QObject *receiver, Fn fn)
{
	QObject::connect(model, &QAbstractItemModel::dataChanged, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::modelReset, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::rowsInserted, receiver, fn);
	QObject::connect(model, &QAbstractItemModel::rowsRemoved, receiver, fn);
}

static QModelIndex
mapFromView(const QAbstractItemView *view, const QModelIndex &index)
{
	if(const auto *model =
		   qobject_cast<const QAbstractProxyModel *>(view->model())) {
		return model->mapToSource(index);
	}
	return index;
}

static QModelIndex
mapToView(const QAbstractItemView *view, const QModelIndex &index)
{
	if(const auto *model =
		   qobject_cast<const QAbstractProxyModel *>(view->model())) {
		return model->mapFromSource(index);
	}
	return index;
}

static void execCanvasShortcutDialog(
	QTableView *view, const CanvasShortcuts::Shortcut *shortcut,
	CanvasShortcutsModel *model, QWidget *parent, const QString &title,
	std::function<QModelIndex(CanvasShortcuts::Shortcut)> &&callback)
{
	dialogs::CanvasShortcutsDialog editor(shortcut, *model, parent);
	editor.setWindowTitle(title);
	editor.setWindowModality(Qt::WindowModal);
	if(editor.exec() == QDialog::Accepted) {
		auto row = std::invoke(callback, editor.shortcut());
		if(row.isValid()) {
			view->selectRow(mapToView(view, row).row());
		}
	}
}

QWidget *Shortcuts::initCanvasShortcuts(
	desktop::settings::Settings &settings, QLineEdit *filter)
{
	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);

	CanvasShortcutsModel *shortcutsModel = new CanvasShortcutsModel(this);
	shortcutsModel->loadShortcuts(settings.canvasShortcuts());
	onAnyModelChange(shortcutsModel, &settings, [=, &settings] {
		settings.setCanvasShortcuts(shortcutsModel->saveShortcuts());
	});

	ProportionalTableView *shortcuts =
		ProportionalTableView::make(filter, shortcutsModel);
	shortcuts->setColumnStretches({6, 3, 3, 0});
	utils::bindKineticScrolling(shortcuts);
	connect(
		shortcuts, &QAbstractItemView::activated, this,
		[=](const QModelIndex &index) {
			if(const auto *shortcut = shortcutsModel->shortcutAt(
				   mapFromView(shortcuts, index).row())) {
				execCanvasShortcutDialog(
					shortcuts, shortcut, shortcutsModel, this,
					tr("Edit Canvas Shortcut"), [=](auto newShortcut) {
						return shortcutsModel->editShortcut(
							*shortcut, newShortcut);
					});
			}
		});
	layout->addWidget(shortcuts, 1);

	utils::EncapsulatedLayout *actions = listActions(
		shortcuts, tr("Add canvas shortcut…"),
		[=] {
			execCanvasShortcutDialog(
				shortcuts, nullptr, shortcutsModel, this,
				tr("New Canvas Shortcut"), [=](auto newShortcut) {
					return shortcutsModel->addShortcut(newShortcut);
				});
		},
		tr("Remove selected canvas shortcut…"),
		makeDefaultDeleter(
			this, shortcuts, tr("Remove canvas shortcut"),
			QT_TR_N_NOOP("Really remove %n canvas shortcut(s)?")));
	actions->addStretch();

	QPushButton *restoreDefaults = new QPushButton(tr("Restore defaults…"));
	restoreDefaults->setAutoDefault(false);
	connect(restoreDefaults, &QPushButton::clicked, this, [=] {
		bool confirm = execConfirm(
			tr("Restore Canvas Shortcut Defaults"),
			tr("Really restore all canvas shortcuts to their default values?"),
			this);
		if(confirm) {
			shortcutsModel->restoreDefaults();
		}
	});
	actions->addWidget(restoreDefaults);
	layout->addLayout(actions);

	return widget;
}

QWidget *Shortcuts::initTouchShortcuts(desktop::settings::Settings &settings)
{
	QScrollArea *scroll = new QScrollArea;
	scroll->setFrameStyle(QScrollArea::NoFrame);
	scroll->setWidgetResizable(true);
	utils::bindKineticScrolling(scroll);

	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);
	scroll->setWidget(widget);

	QFormLayout *modeForm = utils::addFormSection(layout);
	QButtonGroup *touchMode = utils::addRadioGroup(
		modeForm, tr("Touch mode:"), true,
		{
			{tr("Touchscreen"), false},
			{tr("Gestures"), true},
		});
	settings.bindTouchGestures(touchMode);

	utils::addFormSeparator(layout);
	QFormLayout *touchForm = utils::addFormSection(layout);

	QComboBox *oneFingerTouch = new QComboBox;
	oneFingerTouch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	oneFingerTouch->addItem(
		tr("No action"), int(desktop::settings::OneFingerTouchAction::Nothing));
	oneFingerTouch->addItem(
		tr("Draw"), int(desktop::settings::OneFingerTouchAction::Draw));
	oneFingerTouch->addItem(
		tr("Pan canvas"), int(desktop::settings::OneFingerTouchAction::Pan));
	oneFingerTouch->addItem(
		tr("Guess"), int(desktop::settings::OneFingerTouchAction::Guess));
	settings.bindOneFingerTouch(oneFingerTouch, Qt::UserRole);
	touchForm->addRow(tr("One-finger touch:"), oneFingerTouch);

	QComboBox *twoFingerPinch = new QComboBox;
	twoFingerPinch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	twoFingerPinch->addItem(
		tr("No action"), int(desktop::settings::TwoFingerPinchAction::Nothing));
	twoFingerPinch->addItem(
		tr("Zoom"), int(desktop::settings::TwoFingerPinchAction::Zoom));
	settings.bindTwoFingerPinch(twoFingerPinch, Qt::UserRole);
	touchForm->addRow(tr("Two-finger pinch:"), twoFingerPinch);

	QComboBox *twoFingerTwist = new QComboBox;
	twoFingerTwist->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	twoFingerTwist->addItem(
		tr("No action"), int(desktop::settings::TwoFingerPinchAction::Nothing));
	twoFingerTwist->addItem(
		tr("Rotate canvas"),
		int(desktop::settings::TwoFingerTwistAction::Rotate));
	twoFingerTwist->addItem(
		tr("Free rotate canvas"),
		int(desktop::settings::TwoFingerTwistAction::RotateNoSnap));
	twoFingerTwist->addItem(
		tr("Ratchet rotate canvas"),
		int(desktop::settings::TwoFingerTwistAction::RotateDiscrete));
	settings.bindTwoFingerTwist(twoFingerTwist, Qt::UserRole);
	touchForm->addRow(tr("Two-finger twist:"), twoFingerTwist);

	utils::addFormSeparator(layout);
	QFormLayout *tapForm = utils::addFormSection(layout);

	QComboBox *oneFingerTap = new QComboBox;
	QComboBox *twoFingerTap = new QComboBox;
	QComboBox *threeFingerTap = new QComboBox;
	QComboBox *fourFingerTap = new QComboBox;
	for(QComboBox *tap :
		{oneFingerTap, twoFingerTap, threeFingerTap, fourFingerTap}) {
		tap->addItem(
			tr("No action"), int(desktop::settings::TouchTapAction::Nothing));
		tap->addItem(tr("Undo"), int(desktop::settings::TouchTapAction::Undo));
		tap->addItem(tr("Redo"), int(desktop::settings::TouchTapAction::Redo));
		tap->addItem(
			tr("Hide docks"),
			int(desktop::settings::TouchTapAction::HideDocks));
		tap->addItem(
			tr("Toggle color picker"),
			int(desktop::settings::TouchTapAction::ColorPicker));
		tap->addItem(
			tr("Toggle eraser"),
			int(desktop::settings::TouchTapAction::Eraser));
		tap->addItem(
			tr("Toggle erase mode"),
			int(desktop::settings::TouchTapAction::EraseMode));
		tap->addItem(
			tr("Toggle recolor mode"),
			int(desktop::settings::TouchTapAction::RecolorMode));
	}

	settings.bindOneFingerTap(oneFingerTap, Qt::UserRole);
	settings.bindTwoFingerTap(twoFingerTap, Qt::UserRole);
	settings.bindThreeFingerTap(threeFingerTap, Qt::UserRole);
	settings.bindFourFingerTap(fourFingerTap, Qt::UserRole);

	settings.bindTouchGestures(twoFingerTap, &QComboBox::setDisabled);
	settings.bindTouchGestures(threeFingerTap, &QComboBox::setDisabled);
	settings.bindTouchGestures(fourFingerTap, &QComboBox::setDisabled);

	tapForm->addRow(tr("One-finger tap:"), oneFingerTap);
	tapForm->addRow(tr("Two-finger tap:"), twoFingerTap);
	tapForm->addRow(tr("Three-finger tap:"), threeFingerTap);
	tapForm->addRow(tr("Four-finger tap:"), fourFingerTap);

	utils::addFormSeparator(layout);
	QFormLayout *tapAndHoldForm = utils::addFormSection(layout);

	QComboBox *oneFingerTapAndHold = new QComboBox;
	for(QComboBox *tapAndHold : {oneFingerTapAndHold}) {
		tapAndHold->addItem(
			tr("No action"),
			int(desktop::settings::TouchTapAndHoldAction::Nothing));
		tapAndHold->addItem(
			tr("Pick color"),
			int(desktop::settings::TouchTapAndHoldAction::ColorPickMode));
	}

	settings.bindOneFingerTapAndHold(oneFingerTapAndHold, Qt::UserRole);
	settings.bindTouchGestures(oneFingerTapAndHold, &QComboBox::setDisabled);
	tapAndHoldForm->addRow(tr("One-finger tap and hold:"), oneFingerTapAndHold);

	layout->addStretch();
	return scroll;
}

} // namespace settingsdialog
} // namespace dialogs
