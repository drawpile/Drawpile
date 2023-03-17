/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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
#ifndef INDEX_FILMSTRIP_WIDGET_H
#define INDEX_FILMSTRIP_WIDGET_H

#include <QWidget>
#include <QCache>

#include <functional>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

class QScrollBar;
class QImage;
class QPixmap;

namespace widgets {

typedef std::function<QImage(int)> LoadImageFn;

/**
 * @brief Filmstrip widget for visualizing recording index
 */
class QDESIGNER_WIDGET_EXPORT Filmstrip final : public QWidget {
	Q_OBJECT
public:
	Filmstrip(QWidget *parent=nullptr);
	~Filmstrip() override;

	//! Set the length of the recording (in arbitrary units)
	void setLength(int len);
	int length() const { return m_length; }

	//! Set cursor position (in the same units as the length)
	void setCursor(int c);
	int cursor() const { return m_cursor; }

	//! Set the number of frames in the strip
	void setFrames(int f);

	//! Set the function used to load frame images
	void setLoadImageFn(LoadImageFn fn) { m_loadimagefn = fn; }

signals:
	//! Doubleclick detected. Pos is in length units.
	void doubleClicked(int pos);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent*) override;
	void wheelEvent(QWheelEvent *) override;

private:
	QSize frameSize() const;
	int cursorPos() const;
	QPixmap getFrame(int idx) const;

	QScrollBar *m_scrollbar;
	LoadImageFn m_loadimagefn;
	mutable QCache<int, QPixmap> m_cache;
	int m_cursor;
	int m_length;
	int m_frames;
};

}

#endif


