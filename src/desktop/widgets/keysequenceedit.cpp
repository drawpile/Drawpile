// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/keysequenceedit.h"

#include <QToolButton>
#include <QHBoxLayout>
#include <QKeySequenceEdit>

namespace widgets {

KeySequenceEdit::KeySequenceEdit(QWidget *parent) : QWidget(parent)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	QToolButton *clearButton = new QToolButton(this);
	clearButton->setText("⌫");

	_edit = new QKeySequenceEdit(this);
	connect(clearButton, &QToolButton::clicked, _edit, &QKeySequenceEdit::clear);
	connect(_edit, &QKeySequenceEdit::editingFinished, this, &KeySequenceEdit::editingFinished);

	layout->addWidget(_edit);
	layout->addWidget(clearButton);

	setFocusProxy(_edit);
}

void KeySequenceEdit::setKeySequence(const QKeySequence &ks)
{
	_edit->setKeySequence(ks);
}

QKeySequence KeySequenceEdit::keySequence() const
{
	return _edit->keySequence();
}

}
