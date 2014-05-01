/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <QDialog>

class Ui_ColorDialog;

namespace dialogs {

/**
 * @brief Color selection dialog
 * The color selection dialog provides sliders for chaning a color's
 * RGB and HSV values.
 */
class ColorDialog : public QDialog
{
Q_OBJECT
public:
	enum Flag {
		NO_OPTIONS = 0x00,
		NO_APPLY = 0x01,
		SHOW_ALPHA = 0x02
	};
	Q_DECLARE_FLAGS(Flags, Flag)

	ColorDialog(QWidget *parent, const QString& title, Flags flags=NO_OPTIONS);
	~ColorDialog();

	//! Set the current color
	void setColor(const QColor& color);

	//! Get the current color
	QColor color() const;

public slots:
	void accept();
	void apply();

	/**
	 * @brief Show the color picker dialog
	 * @param oldcolor the currently selected color
	 */
	void pickNewColor(const QColor &oldcolor);

signals:
	/**
	 * @brief Apply or Ok was pressed
	 * @param color the new selected color
	 */
	void colorSelected(const QColor& color);

private slots:
	void reset();
	void updateRgb();
	void updateHsv();
	void updateTriangle(const QColor& color);
	void updateHex();

private:
	void updateBars();
	void updateCurrent(const QColor& color);

	Ui_ColorDialog *_ui;
	int _validhue;
	bool _showalpha;
	bool _updating;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ColorDialog::Flags)

}

#endif

