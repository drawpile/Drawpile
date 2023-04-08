// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLMESSAGE_H
#define TOOLMESSAGE_H

#include <QLabel>

// A tooltip-like message to show messages from drawing tools. For example, the
// floodfill tool tells the user if the size limit was exceeded through this.
class ToolMessage final : public QLabel {
	Q_OBJECT
public:
	static void showText(const QString &text);

protected:
	virtual void timerEvent(QTimerEvent *e) override;

private:
	ToolMessage(const QString &text);
};

#endif
