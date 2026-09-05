// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_BANNER_H
#define DESKTOP_WIDGETS_BANNER_H
#include <QWidget>

namespace widgets {

class Banner final : public QWidget {
	Q_OBJECT
public:
	explicit Banner(
		const QIcon &icon, const QString &text, Qt::TextFormat textFormat,
		bool dismissable, QWidget *parent = nullptr);

Q_SIGNALS:
	void linkActivated(const QString &link);
};

}

#endif
