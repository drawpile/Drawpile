// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/imageresourcetextbrowser.h"

namespace widgets {

ImageResourceTextBrowser::ImageResourceTextBrowser(QWidget *parent)
	: QTextBrowser(parent)
{
}

void ImageResourceTextBrowser::clearImages()
{
	m_images.clear();
}

void ImageResourceTextBrowser::setImage(
	const QString &name, const QImage &image)
{
	if(image.isNull()) {
		m_images.remove(name);
	} else {
		m_images.insert(name, image);
	}
}

QVariant ImageResourceTextBrowser::loadResource(int type, const QUrl &name)
{
	if(type == QTextDocument::ImageResource) {
		QHash<QString, QImage>::const_iterator it =
			m_images.constFind(name.toString());
		if(it != m_images.constEnd()) {
			return *it;
		}
	}
	return QTextBrowser::loadResource(type, name);
}

}
