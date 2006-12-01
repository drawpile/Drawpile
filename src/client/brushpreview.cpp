#include <QPainter>

#include "brushpreview.h"

namespace widgets {

BrushPreview::BrushPreview(QWidget *parent)
	: QWidget(parent)
{
	//setMinimumSize(32,32);
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	painter.fillRect(contentsRect(), palette().window());

	QPixmap brush = brush_.getBrush(1.0);

	painter.drawPixmap(0,0,brush);

}


}

