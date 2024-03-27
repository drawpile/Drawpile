// SPDX-License-Identifier: GPL-3.0-or-later
#include <QWidget>

class QIcon;
class QLabel;
class QPushButton;
class QTimer;

namespace widgets {

class NotificationBar final : public QWidget {
	Q_OBJECT
public:
	enum class RoleColor { Notice, Warning };

	explicit NotificationBar(QWidget *parent);

	void show(
		const QString &text, const QIcon &actionButtonIcon,
		const QString &actionButtonLabel, RoleColor color,
		bool allowDismissal = true);

	void setActionButtonEnabled(bool enabled);
	bool isActionButtonEnabled() const;

	void startAutoDismissTimer();

public slots:
	void cancelAutoDismissTimer();

signals:
	void actionButtonClicked();
	void closeButtonClicked();
	void heightChanged(int height);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void paintEvent(QPaintEvent *) override;
	void showEvent(QShowEvent *) override;
	void hideEvent(QHideEvent *) override;

private slots:
	void tickAutoDismiss();

private:
	static constexpr int AUTO_DISMISS_SECONDS = 10;

	void updateCloseButtonText();
	void setColor(const QColor &color, const QString &textColor);
	void updateSize(const QSize &parentSize);

	QColor m_color;
	QLabel *m_label;
	QPushButton *m_actionButton;
	QPushButton *m_closeButton;
	int m_dismissSeconds;
	QTimer *m_dismissTimer = nullptr;
};

}
