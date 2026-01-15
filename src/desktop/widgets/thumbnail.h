// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFrame>
#include <QPixmap>

namespace widgets {

class Thumbnail final : public QFrame {
	Q_OBJECT
public:
	explicit Thumbnail(QWidget *parent = nullptr);
	explicit Thumbnail(const QPixmap &pixmap, QWidget *parent = nullptr);

	void setPixmap(const QPixmap &pixmap);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void paintPixmap();

	QPixmap m_pixmap;
};

}
