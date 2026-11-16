// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_CONFIG_COLORWHEELCONFIG
#define DESKTOP_CONFIG_COLORWHEELCONFIG
#include <QObject>

namespace color_widgets {
class ColorDialog;
class ColorWheel;
}

namespace config {

class Config;

class ColorWheelConfig : public QObject {
	Q_OBJECT
public:
	static constexpr int TYPE_ROTATING_TRIANGLE = 0;
	static constexpr int TYPE_ROTATING_SQUARE = 1;
	static constexpr int TYPE_UPRIGHT_TRIANGLE = 2;
	static constexpr int TYPE_SIDEWAYS_TRIANGLE = 3;
	static constexpr int TYPE_FIXED_SQUARE = 4;

	static constexpr int SHAPE_TRIANGLE = 0;
	static constexpr int SHAPE_SQUARE = 1;

	static constexpr int ANGLE_FIXED = 0;
	static constexpr int ANGLE_ROTATING = 1;
	static constexpr int ANGLE_FIXED_SIDEWAYS = 2;

	ColorWheelConfig(Config *cfg, QObject *parent = nullptr);

	int type() const { return m_type; }
	void setType(int type);

	void applyToColorDialog(color_widgets::ColorDialog *dlg) const;
	void applyToColorWheel(color_widgets::ColorWheel *wheel) const;

Q_SIGNALS:
	void wheelConfigChanged();

private:
	void setShapeFromConfig(int shape);
	void setAngleFromConfig(int angle);
	void setMirrorFromConfig(bool mirror);

	void updateWheelConfig();

	Config *m_cfg;
	int m_type = 0;
	int m_shape = 0;
	int m_angle = 0;
	int m_wheelSelectorShape = 0;
	int m_wheelFixedSelectorAngle = 0;
	bool m_wheelRotatingSelector = true;
	bool m_wheelMirroredSelector = false;
	bool m_mirror = false;
	bool m_updating = true;
};

}

#endif
