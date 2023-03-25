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
#ifndef TOOLMESSAGE_H
#define TOOLMESSAGE_H

#include <QLabel>

// A tooltip-like message to show messages from drawing tools. For example, the
// floodfill tool tells the user if the size limit was exceeded through this.
class ToolMessage final : public QLabel {
	Q_OBJECT
public:
	static void showText(const QString &text);

protected:
	virtual void timerEvent(QTimerEvent *e) override;

private:
	ToolMessage(const QString &text);
};

#endif
