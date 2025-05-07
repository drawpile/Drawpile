// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_RESIZEDIALOG_H
#define DESKTOP_DIALOGS_RESIZEDIALOG_H
#include <QDialog>

class QAction;
class Ui_ResizeDialog;

namespace dialogs {

struct ResizeVector {
	int top, right, bottom, left;

	bool isZero() const
	{
		return top == 0 && right == 0 && bottom == 0 && left == 0;
	}
};

class ResizeDialog final : public QDialog {
	Q_OBJECT
public:
	enum class ExpandDirection { None, Up, Left, Right, Down };

	ResizeDialog(
		const QSize &oldsize, const QAction *expandUpAction,
		const QAction *expandLeftAction, const QAction *expandRightAction,
		const QAction *expandDownAction, QWidget *parent = nullptr);
	~ResizeDialog() override;

	void setBackgroundColor(const QColor &bgColor);
	void setPreviewImage(const QImage &image);
	void setBounds(const QRect &rect, bool clamp);
	void setCompatibilityMode(bool compatibilityMode);
	void initialExpand(int expandDirection);

	QSize newSize() const;
	QPoint newOffset() const;
	QRect newBounds() const;
	ResizeVector resizeVector() const;

	static bool checkDimensions(
		long long width, long long height, bool compatibilityMode,
		QString *outError = nullptr);

public slots:
	void done(int r) override;

private slots:
	void widthChanged(int);
	void heightChanged(int);
	void toggleAspectRatio(bool keep);
	void expandUp();
	void expandLeft();
	void expandRight();
	void expandDown();
	void reset();

private:
	static constexpr int EXPAND = 64;

	void updateError();

	Ui_ResizeDialog *m_ui;

	QSize m_oldsize;
	qreal m_aspectratio = 0.0;
	int m_lastchanged = 0;
	bool m_compatibilityMode = false;
};

}

#endif
