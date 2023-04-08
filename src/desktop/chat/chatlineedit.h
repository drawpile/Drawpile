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

	//! Get the current text with trailing whitespace removed
	QString trimmedText() const;

signals:
	void returnPressed(const QString &text);

public slots:

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QStringList _history;
	QString _current;
	int _historypos;

   void resizeBasedOnLines();
   int lineCountToWidgetHeight(int lineCount) const;
};

#endif
