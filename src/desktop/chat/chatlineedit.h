// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_CHAT_CHATLINEEDIT_H
#define DESKTOP_CHAT_CHATLINEEDIT_H
#include <QPlainTextEdit>
#include <QStringList>

namespace utils {
class KineticScroller;
}

class ChatLineEdit final : public QPlainTextEdit {
	Q_OBJECT
public:
	explicit ChatLineEdit(QWidget *parent = nullptr);

	void pushHistory(const QString &text);

signals:
	void messageSent(const QString &text);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void fixScroll(int value);
	void resizeBasedOnLines();
	int lineCountToWidgetHeight(int lineCount) const;
	void fixScrollAt(int value, int lineCount);

	QStringList m_history;
	QString m_current;
	int m_historypos = 0;
	bool m_fixingScroll = false;
	utils::KineticScroller *m_kineticScroller;
};

#endif
