/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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

#include "colorbutton.h"

#ifndef DESIGNER_PLUGIN
#include "dialogs/colordialog.h"
#endif

#include <QStylePainter>
#include <QStyleOptionButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

namespace widgets {

ColorButton::ColorButton(QWidget *parent,const QColor& color)
	: QToolButton(parent), _color(color), _setAlpha(false), _locked(false)
{
	setAcceptDrops(true);

	connect(this, SIGNAL(clicked()), this, SLOT(selectColor()));
}

void ColorButton::setColor(const QColor& color)
{
	_color = color;
	update();
}

void ColorButton::setAlpha(bool use)
{
	_setAlpha = use;
}

void ColorButton::selectColor()
{
#ifndef DESIGNER_PLUGIN
	if(!_locked) {
		color_widgets::ColorDialog dlg;
		dialogs::applyColorDialogSettings(&dlg);
		dlg.setWindowTitle(tr("Select a color"));
		dlg.setAlphaEnabled(alpha());
		dlg.setButtonMode(color_widgets::ColorDialog::OkCancel);
		dlg.setColor(color());
		if(dlg.exec() == QDialog::Accepted) {
			if(dlg.color() != color()) {
				setColor(dlg.color());
				emit colorChanged(color());
			}
		}
	}
#endif
}

/**
 * Draw widget contents on screen
 * @param event event info
 */
void ColorButton::paintEvent(QPaintEvent *e)
{
	// this is based on QtColorButton from Qt tools source
	QToolButton::paintEvent(e);

	const int pixSize = 10;

	QBrush br(color());
	if (_setAlpha) {
		QPixmap pm(2 * pixSize, 2 * pixSize);
		QPainter pmp(&pm);
		pmp.fillRect(0, 0, pixSize, pixSize, Qt::white);
		pmp.fillRect(pixSize, pixSize, pixSize, pixSize, Qt::white);
		pmp.fillRect(0, pixSize, pixSize, pixSize, Qt::black);
		pmp.fillRect(pixSize, 0, pixSize, pixSize, Qt::black);
		pmp.fillRect(0, 0, 2 * pixSize, 2 * pixSize, color());
		br = QBrush(pm);
	}

	QPainter p(this);
	const int corr = 4;
	QRect r = rect().adjusted(corr, corr, -corr, -corr);
	p.setBrushOrigin((r.width() % pixSize + pixSize) / 2 + corr, (r.height() % pixSize + pixSize) / 2 + corr);
	p.fillRect(r, br);

	const QColor frameColor1(0, 0, 0, 26);
	p.setPen(frameColor1);
	p.drawRect(r.adjusted(1, 1, -2, -2));
	const QColor frameColor2(0, 0, 0, 51);
	p.setPen(frameColor2);
	p.drawRect(r.adjusted(0, 0, -1, -1));
}

/**
 * @brief accept color drops
 * @param event event info
 */
void ColorButton::dragEnterEvent(QDragEnterEvent *event)
{
	if(!_locked) {
		if(event->mimeData()->hasColor())
			event->acceptProposedAction();
	}
}

/**
 * @brief handle color drops
 * @param event event info
 */
void ColorButton::dropEvent(QDropEvent *event)
{
	if(!_locked) {
		const QColor col = qvariant_cast<QColor>(event->mimeData()->colorData());
		setColor(col);
		emit colorChanged(col);
	}
}

}

