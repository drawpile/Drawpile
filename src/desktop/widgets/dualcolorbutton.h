// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_DUALCOLORBUTTON
#define DESKTOP_WIDGETS_DUALCOLORBUTTON
#include <QColor>
#include <QWidget>

class QHelpEvent;

namespace widgets {

class DualColorButton final : public QWidget {
	Q_OBJECT
public:
	DualColorButton(QWidget *parent = nullptr);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

	static QString foregroundText();
	static QString backgroundText();
	static QString resetText();
	static QString swapText();

public slots:
	void setForegroundColor(const QColor &foregroundColor);
	void setBackgroundColor(const QColor &backgroundColor);

signals:
    void foregroundClicked();
    void backgroundClicked();
    void resetClicked();
    void swapClicked();

protected:
	bool event(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	static constexpr qreal COLOR_RECT_RATIO = 19.0 / 32.0;
	static constexpr qreal RESET_RECT_RATIO = 8.0 / 32.0;
	enum class Action { Foreground, Background, Reset, Swap, None };

    void updateHoveredAction(Action hoveredAction);
	QString getToolTipForAction(Action action);
	Action getActionAt(const QPointF &pos);

	Action m_hoveredAction = Action::None;
	QColor m_foregroundColor = Qt::black;
	QColor m_backgroundColor = Qt::white;
};

}

#endif
