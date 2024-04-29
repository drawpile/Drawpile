// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/kis_slider_spin_box.h"

namespace widgets {

class ZoomSlider : public KisDoubleSliderSpinBox {
	Q_OBJECT
public:
	ZoomSlider(QWidget *parent);

	void stepBy(int steps) override;

signals:
	void zoomStepped(int steps);
};

}
