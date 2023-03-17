/*
   Drawpile - a collaborative drawing program.

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
#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QToolButton>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

namespace widgets {

//! A button for selecting a color
class QDESIGNER_WIDGET_EXPORT ColorButton final : public QToolButton {
Q_OBJECT
Q_PROPERTY(QColor color READ color WRITE setColor)
Q_PROPERTY(bool setAlpha READ alpha WRITE setAlpha)
Q_PROPERTY(bool locked READ locked WRITE setLocked)
public:
	ColorButton(QWidget *parent=nullptr, const QColor& color = Qt::black);

	//! Get the selected color
	QColor color() const { return _color; }

	//! Allow setting of alpha value
	void setAlpha(bool use);

	//! Set alpha value?
	bool alpha() const { return _setAlpha; }

	//! Lock color changing
	void setLocked(bool locked) { _locked = locked; }

	bool locked() const { return _locked; }

public slots:
	//! Set color selection
	void setColor(const QColor& color);

signals:
	void colorChanged(const QColor& color);

private slots:
	void selectColor();

protected:
	void paintEvent(QPaintEvent *) override;
	void dragEnterEvent(QDragEnterEvent *) override;
	void dropEvent(QDropEvent *) override;

private:
	QColor _color;
	bool _setAlpha;
	bool _locked;
};

}

#endif
