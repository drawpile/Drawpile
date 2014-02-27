/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#ifndef DP_SESSION_LOADER_H
#define DP_SESSION_LOADER_H

#include <QSize>
#include <QColor>
#include <QString>
#include <QImage>

#include "../shared/net/message.h"

namespace drawingboard {
	class CanvasScene;
}

/**
 * \brief Base class for session initializers.
 * 
 * Note. The initializers make two assumptions: they are running in
 * loopback mode and that the session has not already been initialized.
 * 
 * These assumptions allow the initializers to pick the initial layer/annotation
 * IDs.
 */
class SessionLoader {
public:
	virtual ~SessionLoader() = default;
	
	/**
	 * @brief Get the commands needed to initialize the session.
	 *
	 * The commands should be sent to the server in response to a snapshot request,
	 * or when initializing the session in local mode.
	 *
	 * @param client
	 * @return empty list if an error occurred
	 */
	virtual QList<protocol::MessagePtr> loadInitCommands() = 0;

	/**
	 * @brief Get the error message
	 *
	 * The error message is available if loadInitCommands() returns false.
	 * @return error message
	 */
	virtual QString errorMessage() const = 0;

	/**
	 * @brief get the name of the file
	 *
	 * This if for image loaders. If there is no file (that can be saved again),
	 * this function should return an empty string.
	 * @return filename or empty string
	 */
	virtual QString filename() const = 0;
};

class BlankCanvasLoader : public SessionLoader {
public:
	BlankCanvasLoader(const QSize &size, const QColor &color) : _size(size), _color(color)
	{}
	
	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return ""; }
	QString errorMessage() const { return ""; /* cannot fail */ }

private:
	QSize _size;
	QColor _color;
};

class ImageCanvasLoader : public SessionLoader {
public:
	ImageCanvasLoader(const QString &filename) : _filename(filename) {}
	
	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return _filename; }
	QString errorMessage() const { return _error; }

private:
	QString _filename;
	QString _error;
};

class QImageCanvasLoader : public SessionLoader {
public:
	QImageCanvasLoader(const QImage &image) : _image(image) {}

	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return ""; }
	QString errorMessage() const { return ""; }

private:
	QImage _image;
};

/**
 * @brief A session loader that takes an existing session and generates a new snapshot from it
 */
class SnapshotLoader : public SessionLoader {
public:
	SnapshotLoader(drawingboard::CanvasScene *scene) : _scene(scene) {}

	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return ""; }
	QString errorMessage() const { return ""; }

private:
	drawingboard::CanvasScene *_scene;
};

#endif
