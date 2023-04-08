// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATWIDGETPINNEDAREA_H
#define CHATWIDGETPINNEDAREA_H

#include <QLabel>

namespace widgets {

class ChatWidgetPinnedArea final : public QLabel
{
	Q_OBJECT
public:
	explicit ChatWidgetPinnedArea(QWidget *parent = nullptr);
	void setPinText(const QString &);

protected:
	void mouseDoubleClickEvent(QMouseEvent *event) override;
};

}

#endif // CHATWIDGETPINNEDAREA_H
