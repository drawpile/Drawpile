// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATLINEEDIT_H
#define CHATLINEEDIT_H

#include <QStringList>
#include <QPlainTextEdit>

/**
 * @brief A specialized line edit widget for chatting, with history
  */
class ChatLineEdit final : public QPlainTextEdit
{
Q_OBJECT
public:
	explicit ChatLineEdit(QWidget *parent = nullptr);

	//! Push text to history
	void pushHistory(const QString& text);

signals:
	void messageSent(const QString &text);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void fixScroll(int value);

private:
	QStringList _history;
	QString _current;
	int _historypos;
	bool _fixingScroll;

   void resizeBasedOnLines();
   int lineCountToWidgetHeight(int lineCount) const;
   void fixScrollAt(int value, int lineCount);
};

#endif
