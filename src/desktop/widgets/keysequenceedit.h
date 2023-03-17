/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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
