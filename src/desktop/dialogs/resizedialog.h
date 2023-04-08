// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QDialog>

class Ui_ResizeDialog;

namespace dialogs {

struct ResizeVector {
	int top, right, bottom, left;

	bool isZero() const {
		return top==0 && right==0 && bottom==0 && left==0;
	}
};

class ResizeDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit ResizeDialog(const QSize &oldsize, QWidget *parent=nullptr);
	~ResizeDialog() override;

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
	float m_aspectratio;
	int m_lastchanged;
};

}

#endif
