// SPDX-License-Identifier: GPL-3.0-or-later

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
