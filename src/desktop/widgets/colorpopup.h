// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_COLOR_POPUP_H
#define DESKTOP_WIDGETS_COLOR_POPUP_H
#include <QColor>
#include <QWidget>

namespace widgets {

class ColorPopup : public QWidget {
	Q_OBJECT
public:
	explicit ColorPopup(QWidget *parent = nullptr);

	void setSelectedColor(const QColor &selectedColor);
	void setPreviousColor(const QColor &previousColor);
	void setLastUsedColor(const QColor &lastUsedColor);

	void
	showPopup(QWidget *p, QWidget *windowOrNull, int yOffset, bool vcenter);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	static QColor sanitizeColor(QColor c);

	static QRect fitIntoWindow(
		const QRect &pr, const QRect &wr, int w, int h, int yOffset,
		bool vcenter);

	static QRect getGlobalGeometry(const QWidget *w);
    static QRect getScreenGeometry(const QWidget *w);

	QColor m_selectedColor = Qt::black;
	QColor m_previousColor = Qt::black;
	QColor m_lastUsedColor = Qt::black;
};

}

#endif
