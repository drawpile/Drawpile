#include "desktop/widgets/zoomslider.h"

namespace widgets {

ZoomSlider::ZoomSlider(QWidget *parent)
	: KisDoubleSliderSpinBox(parent)
{
}

void ZoomSlider::stepBy(int steps)
{
	if(steps != 0) {
		emit zoomStepped(steps);
	}
}

}
