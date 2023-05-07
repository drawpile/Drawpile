// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/canvasshortcutedit.h"
#include "libclient/canvas/canvasshortcuts.h"
#include "libclient/utils/canvasshortcutsmodel.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace widgets {

CanvasShortcutEdit::CanvasShortcutEdit(QWidget *parent)
	: QWidget{parent}
	, m_capturing{false}
	, m_type{CanvasShortcuts::NO_TYPE}
	, m_mods{}
	, m_keys{}
	, m_button{Qt::NoButton}
	, m_capturedMods{}
	, m_capturedKeys{}
	, m_capturedButton{Qt::NoButton}
	, m_lineEdit{new QLineEdit{this}}
	, m_editButton{new QPushButton{this}}
	, m_description{new QLabel{this}}
{
	setFocusPolicy(Qt::StrongFocus);

	QVBoxLayout *outer = new QVBoxLayout;
	outer->setContentsMargins(0, 0, 0, 0);
	setLayout(outer);

	QHBoxLayout *inner = new QHBoxLayout;
	outer->addLayout(inner);
	inner->setContentsMargins(0, 0, 0, 0);
	inner->setSpacing(0);

	m_lineEdit->setEnabled(false);
	m_lineEdit->setFocusProxy(this);
	m_lineEdit->installEventFilter(this);
	updateLineEditText();
	inner->addWidget(m_lineEdit, 1);

	updateEditButton();
	inner->addWidget(m_editButton);

	m_description->setWordWrap(true);
	updateDescription();
	outer->addWidget(m_description);

	connect(
		m_editButton, &QPushButton::clicked, this,
		&CanvasShortcutEdit::toggleEdit);
}

Qt::KeyboardModifiers CanvasShortcutEdit::mods() const
{
	return m_mods;
}

void CanvasShortcutEdit::setMods(Qt::KeyboardModifiers mods)
{
	m_mods = mods;
	updateShortcut();
}

const QSet<Qt::Key> CanvasShortcutEdit::keys() const
{
	return m_keys;
}

void CanvasShortcutEdit::setKeys(const QSet<Qt::Key> &keys)
{
	m_keys = keys;
	updateShortcut();
}

Qt::MouseButton CanvasShortcutEdit::button() const
{
	return m_button;
}

void CanvasShortcutEdit::setButton(Qt::MouseButton button)
{
	m_button = button;
	updateShortcut();
	updateLineEditText();
}

unsigned int CanvasShortcutEdit::type() const
{
	return m_type;
}

void CanvasShortcutEdit::setType(unsigned int type)
{
	m_type = type;
	updateShortcut();
	updateLineEditText();
}

bool CanvasShortcutEdit::event(QEvent *event)
{
	switch(event->type()) {
	case QEvent::Shortcut:
		return true;
	case QEvent::ShortcutOverride:
		event->accept();
		return true;
	default:
		break;
	}
	return QWidget::event(event);
}

bool CanvasShortcutEdit::eventFilter(QObject *obj, QEvent *event)
{
	switch(event->type()) {
	case QEvent::MouseButtonPress:
		mousePressEvent(static_cast<QMouseEvent *>(event));
		return true;
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
		return true;
	case QEvent::Wheel:
		wheelEvent(static_cast<QWheelEvent *>(event));
		return true;
	case QEvent::KeyPress:
		keyPressEvent(static_cast<QKeyEvent *>(event));
		return true;
	case QEvent::KeyRelease:
		keyReleaseEvent(static_cast<QKeyEvent *>(event));
		return true;
	default:
		break;
	}
	return QWidget::eventFilter(obj, event);
}

void CanvasShortcutEdit::keyPressEvent(QKeyEvent *event)
{
	if(m_capturing && !event->isAutoRepeat()) {
		event->accept();
		if(event->key() == Qt::Key_Escape) {
			setCapturing(false, false);
		} else {
			m_capturedMods = event->modifiers();
			if(looksLikeValidKey(event->key())) {
				if(m_type == CanvasShortcuts::KEY_COMBINATION) {
					m_capturedKeys = {Qt::Key(event->key())};
					setCapturing(false, true);
				} else {
					m_capturedKeys.insert(Qt::Key(event->key()));
				}
			}
			updateLineEditText();
		}
	}
}

void CanvasShortcutEdit::keyReleaseEvent(QKeyEvent *event)
{
	if(m_capturing && !event->isAutoRepeat()) {
		event->accept();
		m_capturedMods = event->modifiers();
		if(looksLikeValidKey(event->key())) {
			m_capturedKeys.remove(Qt::Key(event->key()));
		}
		updateLineEditText();
	}
}

void CanvasShortcutEdit::mousePressEvent(QMouseEvent *event)
{
	if(m_capturing) {
		event->accept();
		if(m_type != CanvasShortcuts::KEY_COMBINATION) {
			m_capturedMods = event->modifiers();
			m_capturedButton = event->button();
			setCapturing(false, true);
		}
	}
}

void CanvasShortcutEdit::wheelEvent(QWheelEvent *event)
{
	if(m_capturing && m_type == CanvasShortcuts::MOUSE_WHEEL) {
		event->accept();
		m_capturedMods = event->modifiers();
		setCapturing(false, true);
	}
}

void CanvasShortcutEdit::toggleEdit()
{
	setCapturing(!m_capturing, false);
}

void CanvasShortcutEdit::setCapturing(bool capturing, bool apply)
{
	m_capturing = capturing;

	if(apply && haveValidCapture()) {
		m_mods = m_capturedMods;
		m_keys = m_capturedKeys;
		m_button = m_capturedButton;
		updateShortcut();
	}
	m_capturedMods = {};
	m_capturedKeys.clear();
	m_capturedButton = Qt::NoButton;

	updateEditButton();
	updateLineEditText();
	updateDescription();
	if(m_capturing) {
		setFocus();
	}
}

bool CanvasShortcutEdit::looksLikeValidKey(int key)
{
	// Filter out unknown and modifier keys, those are treated differently.
	// It doesn't look like Qt has a way to do this by itself.
	switch(key) {
	case 0:
	case Qt::Key_unknown:
	case Qt::Key_Alt:
	case Qt::Key_AltGr:
	case Qt::Key_Control:
	case Qt::Key_Meta:
	case Qt::Key_Shift:
		return false;
	default:
		return true;
	}
}

bool CanvasShortcutEdit::haveValidCapture()
{
	CanvasShortcuts::Shortcut s = {
		CanvasShortcuts::Type(m_type),
		m_capturedMods,
		m_capturedKeys,
		m_capturedButton,
		CanvasShortcuts::NO_ACTION,
		CanvasShortcuts::NORMAL,
	};
	return s.isValid(false);
}

void CanvasShortcutEdit::updateShortcut()
{
	// Set a sensible default mouse button so that the line edit doesn't say
	// something weird like "Ctrl+Shift+Unset".
	if(m_type == CanvasShortcuts::MOUSE_BUTTON && m_button == Qt::NoButton) {
		m_button = Qt::MiddleButton;
	}
	emit shortcutChanged();
}

void CanvasShortcutEdit::updateEditButton()
{
	if(m_capturing) {
		m_editButton->setText(tr("Cancel"));
	} else {
		m_editButton->setText(tr("Set..."));
	}
}

void CanvasShortcutEdit::updateLineEditText()
{
	if(m_capturing) {
		m_lineEdit->setEnabled(true);
		m_lineEdit->setText(CanvasShortcutsModel::shortcutToString(
			CanvasShortcuts::NO_TYPE, m_capturedMods, m_capturedKeys,
			m_capturedButton));
	} else {
		m_lineEdit->setEnabled(false);
		m_lineEdit->setText(CanvasShortcutsModel::shortcutToString(
			m_type, m_mods, m_keys, m_button));
	}
	m_lineEdit->setCursor(QCursor{
		m_capturing && m_type != CanvasShortcuts::KEY_COMBINATION
			? Qt::CrossCursor
			: Qt::ArrowCursor});
}

void CanvasShortcutEdit::updateDescription()
{
	QString description;
	if(m_capturing) {
		switch(m_type) {
		case CanvasShortcuts::KEY_COMBINATION:
			description = tr("Press a key combination. Hit Escape to cancel.");
			break;
		case CanvasShortcuts::MOUSE_BUTTON:
			description =
				tr("Click the desired mouse button into the field above, "
				   "optionally while holding down keys. Hit Escape to cancel.");
			break;
		case CanvasShortcuts::MOUSE_WHEEL:
			description =
				tr("Turn the mouse wheel or click into in the field above, "
				   "optionally while holding down keys. Hit Escape to cancel.");
			break;
		case CanvasShortcuts::CONSTRAINT_KEY_COMBINATION:
			description = tr("Hold down the desired key combination and click "
							 "into the field above. Hit Escape to cancel.");
			break;
		default:
			description = tr("Unknown shortcut type %1. Hit Escape to cancel.")
							  .arg(m_type);
			break;
		}
	} else {
		description = tr("Press the Set button to assign a shortcut.");
	}
	m_description->setText(description);
}

}
