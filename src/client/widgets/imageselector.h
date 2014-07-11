/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef IMAGESELECTOR_H
#define IMAGESELECTOR_H

#include <QFrame>
#include <QImage>

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

/**
 * @brief Image selector widget
 * The image selector allows the user to choose which image to use.
 * Options are original image, new image or solid color.
 */
class PLUGIN_EXPORT ImageSelector : public QFrame {
	Q_OBJECT
	public:
		ImageSelector(QWidget *parent=0, Qt::WindowFlags f=0);
		~ImageSelector() {}

		//! Get the selected image
		QImage image() const;

		//! Get the selected image file (if isImageFile()==true)
		const QString &imageFile() const { return _imagefile; }

		//! Return the selected color
		const QColor& color() const { return _color; }

		//! Is solid color currently selected
		bool isColor() const { return _mode == COLOR; }

		//! Is the original image currently selected
		bool isOriginal() const { return _mode == ORIGINAL; }

		//! Is image file currently selected
		bool isImageFile() const { return _mode == IMAGEFILE; }

	public slots:
		void setOriginal(const QImage& image);
		void setImage(const QImage& image);
		void setImage(const QString &filename);
		void setColor(const QColor& color);
		void setWidth(int w);
		void setHeight(int h);

		void chooseOriginal();
		void chooseColor();
		void chooseImage();
	signals:
		void widthChanged(int w);
		void heightChanged(int h);
		void colorDropped();
		void imageDropped();
		void noImageSet();

	protected:
		void paintEvent(QPaintEvent *event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);
		void resizeEvent(QResizeEvent *);

	private:
		void updateCache(const QImage& src);
		QImage _image;
		QString _imagefile;
		QImage _original;
		QPixmap _cache;
		QColor _color;
		QSize _size;

		enum {
			ORIGINAL,
			IMAGE,
			IMAGEFILE,
			COLOR
		} _mode;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

