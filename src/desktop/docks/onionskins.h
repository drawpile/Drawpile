// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ONIONSKINSDOCK_H
#define ONIONSKINSDOCK_H

#include "desktop/docks/dockbase.h"
#include <QPair>
#include <QVector>
#include <functional>

namespace docks {


class OnionSkinsDock final : public DockBase {
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
	void frameCountChanged(int value);

private:
	static int getSliderDefault(int frameNumber);
	void buildWidget();
	void onSliderValueChange(int frameNumber, int value);
	void showSliderValue(int value);
	void showColorPicker(
		const QColor &currentColor,
		std::function<void(QColor)> onColorSelected);

	struct Private;
	Private *d;
};

}

#endif
