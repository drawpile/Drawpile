// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SUBHEADERWIDGET_H
#define SUBHEADERWIDGET_H

#include <QLabel>

namespace server {
namespace gui {

class SubheaderWidget final : public QLabel
{
	Q_OBJECT
public:
	SubheaderWidget(const QString &text, int level, QWidget *parent = nullptr);

protected:
	void paintEvent(QPaintEvent*) override;
};

}
}

#endif
