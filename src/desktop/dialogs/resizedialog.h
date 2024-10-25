// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_RESIZEDIALOG_H
#define DESKTOP_DIALOGS_RESIZEDIALOG_H
#include <QDialog>

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
	explicit ResizeDialog(const QSize &oldsize, QWidget *parent = nullptr);
	~ResizeDialog() override;

	void setBackgroundColor(const QColor &bgColor);
	void setPreviewImage(const QImage &image);
	void setBounds(const QRect &rect);

	QSize newSize() const;
	QPoint newOffset() const;
	ResizeVector resizeVector() const;

public slots:
	void done(int r) override;

private slots:
	void widthChanged(int);
	void heightChanged(int);
	void toggleAspectRatio(bool keep);
	void reset();

private:
	Ui_ResizeDialog *m_ui;

	QSize m_oldsize;
	qreal m_aspectratio = 0.0;
	int m_lastchanged = 0;
};

}

#endif
