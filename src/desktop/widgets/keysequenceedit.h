// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYSEQUENCEEDIT_H
#define KEYSEQUENCEEDIT_H

#include <QWidget>

class QKeySequenceEdit;
class QKeySequence;

namespace widgets {

/**
 * @brief A wrapper for QKeySequenceEdit that adds a clear button
 *
 * This can be removed once the clear button is implemented in QKeySequenceEdit.
 */
class KeySequenceEdit final : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QKeySequence keySequence READ keySequence WRITE setKeySequence USER true)
public:
	explicit KeySequenceEdit(QWidget *parent = nullptr);

	void setKeySequence(const QKeySequence &ks);
	QKeySequence keySequence() const;

signals:
	void editingFinished();

private:
	QKeySequenceEdit *_edit;
};

}

#endif // KEYSEQUENCEEDIT_H
