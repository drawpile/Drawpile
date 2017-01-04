/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef DP_SESSION_LOADER_H
#define DP_SESSION_LOADER_H

#include <QSize>
#include <QColor>
#include <QString>
#include <QImage>

#include "../shared/net/message.h"

namespace paintcore {
	class LayerStack;
}

namespace canvas {

class CanvasModel;

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
	virtual ~SessionLoader() {}
	
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
	 * @brief Get the warning message (if any)
	 */
	virtual QString warningMessage() const { return QString(); }

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
	QString filename() const { return QString(); }
	QString errorMessage() const { return QString(); /* cannot fail */ }

private:
	QSize _size;
	QColor _color;
};

class ImageCanvasLoader : public SessionLoader {
public:
	ImageCanvasLoader(const QString &filename) : m_filename(filename) {}
	
	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return m_filename; }
	QString errorMessage() const { return m_error; }
	QString warningMessage() const { return m_error; }

private:
	QString m_filename;
	QString m_error;
	QString m_warning;
};

class QImageCanvasLoader : public SessionLoader {
public:
	QImageCanvasLoader(const QImage &image) : _image(image) {}

	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return QString(); }
	QString errorMessage() const { return QString(); }

private:
	QImage _image;
};

/**
 * @brief A session loader that takes an existing layer stack and generates a new snapshot from it
 *
 * If the optional canvas is given, extra data will be included.
 */
class SnapshotLoader : public SessionLoader {
public:
	SnapshotLoader(uint8_t contextId, const paintcore::LayerStack *layers, const canvas::CanvasModel *session=nullptr)
		: m_layers(layers), m_session(session), m_contextId(contextId) {}

	QList<protocol::MessagePtr> loadInitCommands();
	QString filename() const { return QString(); }
	QString errorMessage() const { return QString(); }

private:
	const paintcore::LayerStack *m_layers;
	const canvas::CanvasModel *m_session;
	uint8_t m_contextId;
};

}

#endif
