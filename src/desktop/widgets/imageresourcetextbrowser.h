// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_IMAGERESOURCETEXTBROWSER
#define DESKTOP_WIDGETS_IMAGERESOURCETEXTBROWSER
#include <QHash>
#include <QImage>
#include <QTextBrowser>

namespace widgets {

class ImageResourceTextBrowser : public QTextBrowser {
	Q_OBJECT
public:
	explicit ImageResourceTextBrowser(QWidget *parent = nullptr);

	void clearImages();
	void setImage(const QString &name, const QImage &image);

protected:
	QVariant loadResource(int type, const QUrl &name) override;

private:
	QHash<QString, QImage> m_images;
};

}

#endif
