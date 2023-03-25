/*
 *  Drawpile - a collaborative drawing program.
 *
 *  Copyright (C) 2023 askmeaboutloom
 *
 *  Drawpile is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Drawpile is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef ONIONSKINSDOCK_H
#define ONIONSKINSDOCK_H

#include <QDockWidget>
#include <QPair>
#include <QVector>
#include <functional>

class QSettings;

namespace docks {


class OnionSkinsDock final : public QDockWidget {
	Q_OBJECT
public:
	OnionSkinsDock(const QString &title, QWidget *parent);
	~OnionSkinsDock() override;

	void triggerUpdate();

signals:
	void onionSkinsChanged(
		const QVector<QPair<float, QColor>> &skinsBelow,
		const QVector<QPair<float, QColor>> &skinsAbove);

protected:
	void timerEvent(QTimerEvent *) override;

private slots:
	void updateSettings();
	void frameCountChanged(int value);

private:
	static int getSliderDefault(int frameNumber);
	void buildWidget(QSettings &settings);
	void onSliderValueChange(const QString &settingsKey, int value);
	void onTintColorChange(const QString &settingsKey, const QColor &color);
	void startDebounceTimer();
	void showSliderValue(int value);
	void showColorPicker(
		const QColor &currentColor,
		std::function<void(QColor)> onColorSelected);

	struct Private;
	Private *d;
};

}

#endif
