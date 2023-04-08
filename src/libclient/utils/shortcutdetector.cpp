// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/shortcutdetector.h"

#include <QEvent>

ShortcutDetector::ShortcutDetector(QObject *parent)
	: QObject(parent), _shortcutSent(false)
{

}

bool ShortcutDetector::eventFilter(QObject *obj, QEvent *event)
{
	if(event->type() == QEvent::Shortcut) {
		_shortcutSent = true;
	}
	return QObject::eventFilter(obj, event);
}
