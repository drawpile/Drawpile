// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLSETTINGS_LASER_H
#define DESKTOP_TOOLSETTINGS_LASER_H
#include "desktop/toolwidgets/toolsettings.h"

class QLabel;
class QStackedWidget;
class Ui_LaserSettings;

namespace tools {

class LaserPointerSettings final : public ToolSettings {
	Q_OBJECT
public:
	LaserPointerSettings(ToolController *ctrl, QObject *parent = nullptr);
	~LaserPointerSettings() override;

	QString toolType() const override { return QStringLiteral("laser"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return false; }
	bool isLocked() override;

	bool pointerTracking() const;

	void setForeground(const QColor &color) override;
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

	void setFeatureAccess(bool featureAccess);
	void setLaserTrailsShown(bool laserTrailsShown);

public slots:
	void pushSettings() override;

signals:
	void pointerTrackingToggled(bool);
	void showLaserTrailsRequested();

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void togglePointerTracking(bool checked);
	void updateWidgets();

	QStackedWidget *m_stack = nullptr;
	Ui_LaserSettings *m_ui = nullptr;
	QLabel *m_permissionDeniedLabel = nullptr;
	QLabel *m_laserTrailsHiddenLabel = nullptr;
	qreal m_quickAdjust1 = 0.0;
	bool m_featureAccess = true;
	bool m_laserTrailsShown = true;
};

}

#endif
