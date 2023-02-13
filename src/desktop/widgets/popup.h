/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef POPUP_H
#define POPUP_H

#include <QFrame>

namespace widgets {

class Popup : public QFrame {
	Q_OBJECT
public:
	Popup(QWidget *parent);

	void setVisibleOn(QWidget *widget, bool visible);
	void showOn(QWidget *widget);

signals:
	void targetChanged(QWidget *widget);

protected:
	void hideEvent(QHideEvent *) override;

private:
	QPoint findPosition(const QWidget *widget) const;

	static bool getScreenArea(const QWidget *widget, QRect &outRect);

	static bool tryPosition(
		const QRect &screenArea, const QSize &popupSize, QPoint &inOutPoint);
};

}

#endif
