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
#ifndef HIDEDOCKTITLEBARSEVENTFILTER_H
#define HIDEDOCKTITLEBARSEVENTFILTER_H

#include <QObject>

class HideDockTitleBarsEventFilter : public QObject {
	Q_OBJECT
public:
	explicit HideDockTitleBarsEventFilter(QObject *parent = nullptr);

signals:
	void setDockTitleBarsHidden(bool hidden);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void checkDockTitleBarsHidden(Qt::KeyboardModifiers mods);

	bool m_wasHidden;
};

#endif
