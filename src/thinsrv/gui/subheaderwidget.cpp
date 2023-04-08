// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/subheaderwidget.h"

#include <QPainter>

namespace server {
namespace gui {

SubheaderWidget::SubheaderWidget(const QString &text, int level, QWidget *parent)
	: QLabel(text, parent)
{
	QFont f = font();
	f.setBold(true);

	if(level<=1)
		f.setPointSizeF(f.pointSizeF()*2);
	else if(level==2)
		f.setPointSizeF(f.pointSizeF()*1.3);

	setFont(f);
	setContentsMargins(5, 5, 5, 5);
}

void SubheaderWidget::paintEvent(QPaintEvent *e)
{
	QLabel::paintEvent(e);
	QPainter p(this);
	p.setPen(palette().color(QPalette::Highlight));
	p.drawLine(0, height()-3, width(), height()-3);
}

}
}
