// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2016 The Qt Company Ltd.
// SPDX-FileComment: Based on QCommandLinkButton.
#ifndef QCOMMANDLINKBUTTON_H
#define QCOMMANDLINKBUTTON_H
#include <QFont>
#include <QPushButton>
#include <QRect>

namespace widgets {

// This is like QCommandLinkButton, but actually functional. The original goofs
// up on Android where it incorrectly uses point sizes for fonts instead of
// pixel sizes and on Chinese Windows it cuts off characters in the description.
class CommandLinkButton final : public QPushButton {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(CommandLinkButton)

public:
	explicit CommandLinkButton(QWidget *parent = nullptr);
	CommandLinkButton(
		const QIcon &icon, const QString &text, const QString &description,
		QWidget *parent = nullptr);

	QString description() const { return m_description; }
	void setDescription(const QString &description);

	QSize sizeHint() const override;
	int heightForWidth(int width) const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;

private:
	static constexpr int MARGIN_TOP = 10;
	static constexpr int MARGIN_LEFT = 7;
	static constexpr int MARGIN_RIGHT = 4;
	static constexpr int MARGIN_BOTTOM = 10;

	QFont titleFont() const;
	QFont descriptionFont() const;
	int textOffset() const;
	int descriptionOffset() const;
	int descriptionHeight(int widgetWidth) const;

	QString m_description;
};

}

#endif
