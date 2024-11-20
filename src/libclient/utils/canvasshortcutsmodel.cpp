// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/canvasshortcutsmodel.h"
#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>

CanvasShortcutsModel::CanvasShortcutsModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

void CanvasShortcutsModel::loadShortcuts(const QVariantMap &cfg)
{
	beginResetModel();
	m_canvasShortcuts = CanvasShortcuts::load(cfg);
	updateConflictRows(false);
	m_hasChanges = false;
	endResetModel();
}

QVariantMap CanvasShortcutsModel::saveShortcuts()
{
	return m_canvasShortcuts.save();
}

void CanvasShortcutsModel::restoreDefaults()
{
	beginResetModel();
	m_canvasShortcuts.clear();
	m_canvasShortcuts.loadDefaults();
	updateConflictRows(false);
	m_hasChanges = true;
	endResetModel();
}

int CanvasShortcutsModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_canvasShortcuts.shortcutsCount();
}

int CanvasShortcutsModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant CanvasShortcutsModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.parent().isValid()) {
		return QVariant();
	}

	int row = index.row();
	const CanvasShortcuts::Shortcut *s = shortcutAt(row);
	if(!s) {
		return QVariant();
	}

	switch(role) {
	case Qt::DisplayRole:
	case Qt::ToolTipRole: {
		QString text;
		switch(index.column()) {
		case int(Shortcut):
			text = shortcutToString(s->type, s->mods, s->keys, s->button);
			break;
		case int(Action):
			text = actionToString(*s);
			break;
		case int(Modifiers):
			text = flagsToString(*s);
			break;
		default:
			break;
		}
		if(role == Qt::ToolTipRole && m_conflictRows.contains(row)) {
			if(text.isEmpty()) {
				//: Tooltip for a keyboard shortcut conflict.
				return tr("Conflict");
			} else {
				//: Tooltip for a keyboard shortcut conflict, %1 is the name or
				//: key sequence of the shortcut in question.
				return tr("%1 (conflict)").arg(text);
			}
		} else {
			return text;
		}
	}
	case Qt::DecorationRole:
		switch(index.column()) {
		case int(Shortcut):
			if(m_conflictRows.contains(row)) {
				return QIcon::fromTheme("dialog-warning");
			} else {
				return QVariant();
			}
		default:
			return QVariant();
		}
	case Qt::TextAlignmentRole:
		switch(index.column()) {
		case int(Shortcut):
		case int(Modifiers):
			return Qt::AlignCenter;
		default:
			return QVariant();
		}
	case Qt::ForegroundRole:
		if(m_conflictRows.contains(row)) {
			return QColor(Qt::white);
		} else {
			return QVariant();
		}
	case Qt::BackgroundRole:
		if(m_conflictRows.contains(row)) {
			return QColor(0xdc3545);
		} else {
			return QVariant();
		}
	case int(FilterRole): {
		QString conflictMarker =
			m_conflictRows.contains(row) ? QStringLiteral("\n\1") : QString();
		return QStringLiteral("action:%1\nshortcut:%2%3")
			.arg(
				actionToString(*s),
				shortcutToString(s->type, s->mods, s->keys, s->button),
				conflictMarker);
	}
	default:
		return QVariant();
	}
}

bool CanvasShortcutsModel::removeRows(
	int row, int count, const QModelIndex &parent)
{
	if(parent.isValid() || count <= 0 || row < 0 ||
	   row + count > m_canvasShortcuts.shortcutsCount()) {
		return false;
	}

	beginRemoveRows(parent, row, row + count - 1);
	m_canvasShortcuts.removeShortcutAt(row, count);
	m_hasChanges = true;
	endRemoveRows();
	updateConflictRows(true);
	return true;
}

QVariant CanvasShortcutsModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal) {
		return QVariant();
	}

	switch(Column(section)) {
	case Shortcut:
		return tr("Shortcut");
	case Action:
		return tr("Action");
	case Modifiers:
		return tr("Modifiers");
	case ColumnCount:
		break;
	}
	return QVariant();
}

Qt::ItemFlags CanvasShortcutsModel::flags(const QModelIndex &) const
{
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

const CanvasShortcuts::Shortcut *CanvasShortcutsModel::shortcutAt(int row) const
{
	return m_canvasShortcuts.shortcutAt(row);
}

QModelIndex
CanvasShortcutsModel::addShortcut(const CanvasShortcuts::Shortcut &s)
{
	if(!s.isValid()) {
		return QModelIndex();
	}

	beginResetModel();
	int row = m_canvasShortcuts.addShortcut(s);
	updateConflictRows(false);
	m_hasChanges = true;
	endResetModel();
	return createIndex(row, 0);
}

QModelIndex CanvasShortcutsModel::editShortcut(
	const CanvasShortcuts::Shortcut &prev, const CanvasShortcuts::Shortcut &s)
{
	if(!s.isValid()) {
		return QModelIndex();
	}

	beginResetModel();
	int row = m_canvasShortcuts.editShortcut(prev, s);
	updateConflictRows(false);
	m_hasChanges = true;
	endResetModel();
	return createIndex(row, 0);
}

const CanvasShortcuts::Shortcut *CanvasShortcutsModel::searchConflict(
	const CanvasShortcuts::Shortcut &s,
	const CanvasShortcuts::Shortcut *except) const
{
	return m_canvasShortcuts.searchConflict(s, except);
}

QString CanvasShortcutsModel::shortcutTitle(
	const CanvasShortcuts::Shortcut *s, bool actionAndFlagsOnly)
{
	if(!s) {
		return QString();
	}

	QString action = actionToString(*s);
	QString flags = flagsToString(*s);
	if(actionAndFlagsOnly) {
		if(flags.isEmpty()) {
			//: Example: "Pan Canvas"
			return tr("%1").arg(action);
		} else {
			//: Example: "Pan Canvas (Inverted)"
			return tr("%1 (%2)").arg(action).arg(flags);
		}
	} else {
		QString ss = shortcutToString(s->type, s->mods, s->keys, s->button);
		if(flags.isEmpty()) {
			//: Example: "Space: Pan Canvas"
			return tr("%1: %2").arg(ss).arg(action);
		} else {
			//: Example: "Space: Pan Canvas (Inverted)"
			return tr("%1: %2 (%3)").arg(ss).arg(action).arg(flags);
		}
	}
}

QString CanvasShortcutsModel::shortcutToString(
	unsigned int type, Qt::KeyboardModifiers mods, const QSet<Qt::Key> &keys,
	Qt::MouseButton button)
{
	QStringList components;
	for(const auto key : keys) {
		components.append(QKeySequence{key}.toString(QKeySequence::NativeText));
	}
	std::sort(components.begin(), components.end());

#ifdef Q_OS_MACOS
	// OSX has different modifier keys and also orders them differently.
	if(mods.testFlag(Qt::ControlModifier)) {
		components.prepend(QStringLiteral("⌘"));
	}
	if(mods.testFlag(Qt::ShiftModifier)) {
		components.prepend(QStringLiteral("⇧"));
	}
	if(mods.testFlag(Qt::AltModifier)) {
		components.prepend(QStringLiteral("⎇"));
	}
	if(mods.testFlag(Qt::MetaModifier)) {
		components.prepend(QStringLiteral("⌃"));
	}
#else
	// Qt translates modifiers in the QShortcut context, so we do too.
	if(mods.testFlag(Qt::ShiftModifier)) {
		components.prepend(QCoreApplication::translate("QShortcut", "Shift"));
	}
	if(mods.testFlag(Qt::AltModifier)) {
		components.prepend(QCoreApplication::translate("QShortcut", "Alt"));
	}
	if(mods.testFlag(Qt::ControlModifier)) {
		components.prepend(QCoreApplication::translate("QShortcut", "Ctrl"));
	}
	if(mods.testFlag(Qt::MetaModifier)) {
		components.prepend(QCoreApplication::translate("QShortcut", "Meta"));
	}
#endif

	if(type == CanvasShortcuts::MOUSE_BUTTON) {
		components.append(mouseButtonToString(button));
	} else if(type == CanvasShortcuts::MOUSE_WHEEL) {
		components.append(tr("Mouse Wheel"));
	}

	//: Joins shortcut components, probably doesn't need to be translated.
	return components.join(tr("+"));
}

void CanvasShortcutsModel::setExternalKeySequences(
	const QSet<QKeySequence> &externalKeySequences)
{
	if(externalKeySequences != m_externalKeySequences) {
		m_externalKeySequences = externalKeySequences;
		updateConflictRows(true);
	}
}

void CanvasShortcutsModel::updateConflictRows(bool emitDataChanges)
{
	QSet<int> conflictRows;
	int rowCount = m_canvasShortcuts.shortcutsCount();
	for(int i = 0; i < rowCount; ++i) {
		const CanvasShortcuts::Shortcut *s = shortcutAt(i);
		if(s && s->keySequences().intersects(m_externalKeySequences)) {
			conflictRows.insert(i);
		}
	}

	if(emitDataChanges) {
		for(int row : conflictRows + m_conflictRows) {
			if(row >= 0 &&
			   (!conflictRows.contains(row) || !m_conflictRows.contains(row))) {
				emit dataChanged(
					createIndex(row, 0), createIndex(row, ColumnCount - 1));
			}
		}
	}

	m_conflictRows.swap(conflictRows);
}

QString CanvasShortcutsModel::mouseButtonToString(Qt::MouseButton button)
{
	switch(button) {
	case Qt::NoButton:
		return tr("Unset");
	case Qt::LeftButton:
		return tr("Left Click");
	case Qt::RightButton:
		return tr("Right Click");
	case Qt::MiddleButton:
		return tr("Middle Click");
	default:
		break;
	}

	using MouseButtonType = std::underlying_type<Qt::MouseButton>::type;
	constexpr MouseButtonType max = Qt::MaxMouseButton;
	int buttonIndex = 0;
	for(MouseButtonType mb = 1, i = 1; mb <= max; mb *= 2, ++i) {
		if(mb == button) {
			buttonIndex = i;
			break;
		}
	}

	if(buttonIndex == 0) {
		return tr("Unknown Button 0x%1").arg(button, 0, 16);
	} else {
		return tr("Button %1").arg(buttonIndex);
	}
}

QString CanvasShortcutsModel::actionToString(const CanvasShortcuts::Shortcut &s)
{
	switch(s.action) {
	case CanvasShortcuts::CANVAS_PAN:
		return tr("Pan Canvas");
	case CanvasShortcuts::CANVAS_ROTATE:
		return tr("Rotate Canvas");
	case CanvasShortcuts::CANVAS_ZOOM:
		return tr("Zoom Canvas");
	case CanvasShortcuts::COLOR_PICK:
		return tr("Pick Color");
	case CanvasShortcuts::LAYER_PICK:
		return tr("Pick Layer");
	case CanvasShortcuts::TOOL_ADJUST:
		return tr("Change Brush Size");
	case CanvasShortcuts::CONSTRAINT:
		switch(s.flags & CanvasShortcuts::CONSTRAINT_MASK) {
		case CanvasShortcuts::TOOL_CONSTRAINT1:
			return tr("Constrain Tool");
		case CanvasShortcuts::TOOL_CONSTRAINT2:
			return tr("Center Tool");
		case CanvasShortcuts::TOOL_CONSTRAINT1 |
			CanvasShortcuts::TOOL_CONSTRAINT2:
			return tr("Constrain and Center Tool");
		default:
			return tr("Unknown Constraint 0x%1").arg(s.flags, 0, 16);
		}
	case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		return tr("Ratchet Rotate Canvas");
	case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		return tr("Free Rotate Canvas");
	case CanvasShortcuts::COLOR_H_ADJUST:
		return tr("Change Color Hue");
	case CanvasShortcuts::COLOR_S_ADJUST:
		return tr("Change Color Saturation");
	case CanvasShortcuts::COLOR_V_ADJUST:
		return tr("Change Color Value");
	default:
		return tr("Unknown Action %1").arg(s.action);
	}
}

QString CanvasShortcutsModel::flagsToString(const CanvasShortcuts::Shortcut &s)
{
	if(s.flags & CanvasShortcuts::INVERTED) {
		if(s.flags & CanvasShortcuts::SWAP_AXES) {
			return tr("Inverted, Swap Axes");
		} else {
			return tr("Inverted");
		}
	} else if(s.flags & CanvasShortcuts::SWAP_AXES) {
		return tr("Swap Axes");
	} else {
		return QString{};
	}
}
