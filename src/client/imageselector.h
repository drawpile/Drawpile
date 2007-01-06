/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen, based on the GTK+ color selector (C) The 
Free Software Foundation

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

//! Image selector widget
/**
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

		//! Return the selected color
		const QColor& color() const { return color_; }

		//! Is solid color currently selected
		bool isColor() const { return mode_ == COLOR; }

		//! Is the original image currently selected
		bool isOriginal() const { return mode_ == ORIGINAL; }

	public slots:
		void setOriginal(const QImage& image);
		void setImage(const QImage& image);
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

	protected:
		void paintEvent(QPaintEvent *event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);
		void resizeEvent(QResizeEvent *);

	private:
		void updateCache(const QImage& src);
		QImage image_;
		QImage original_;
		QPixmap cache_;
		QColor color_;
		QSize size_;
		enum {ORIGINAL,IMAGE,COLOR} mode_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

