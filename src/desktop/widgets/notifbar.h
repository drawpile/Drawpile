/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QWidget>

class QLabel;
class QPushButton;

namespace widgets {

class NotificationBar final : public QWidget
{
	Q_OBJECT
public:
	enum class RoleColor {
		Warning,
		Fatal
	};

	NotificationBar(QWidget *parent);

	void show(const QString &text, const QString &actionButtonLabel, RoleColor color);

signals:
	void actionButtonClicked();

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void paintEvent(QPaintEvent *) override;
	void showEvent(QShowEvent *) override;
	void hideEvent(QHideEvent *) override;

private:
	void setColor(const QColor &color);
	void updateSize(const QSize &parentSize);

	QColor m_color;
	QLabel *m_label;
	QPushButton *m_actionButton;
	QPushButton *m_closeButton;
};

}
