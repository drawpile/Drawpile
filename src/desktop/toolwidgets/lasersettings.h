// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_LASER_H
#define TOOLSETTINGS_LASER_H

#include "desktop/toolwidgets/toolsettings.h"

class Ui_LaserSettings;

namespace tools {

class LaserPointerSettings final : public ToolSettings {
	Q_OBJECT
public:
	LaserPointerSettings(ToolController *ctrl, QObject *parent=nullptr);
	~LaserPointerSettings() override;

	QString toolType() const override { return QStringLiteral("laser"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return false; }

	bool pointerTracking() const;

	void setForeground(const QColor& color) override;
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

public slots:
	void pushSettings() override;

signals:
	void pointerTrackingToggled(bool);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	Ui_LaserSettings * _ui;
	qreal m_quickAdjust1 = 0.0;
};

}

#endif

