/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#ifndef LAYERSTACKPIXMAP_H
#define LAYERSTACKPIXMAP_H

#include <QObject>
#include <QPixmap>

#include "layerlist.h"
#include "net/envelope.h"

namespace rustpile {
	struct PaintEngine;
	struct Rectangle;
	struct Size;
	struct LayerInfo;
	struct Annotations;
	struct AnnotationAt;
	enum class LayerViewMode;
}

Q_DECLARE_OPAQUE_POINTER(rustpile::Annotations*)

namespace canvas {

/**
 * @brief A Qt compatibility wrapper around the Rust paint engine library.
 *
 * The canvas view cache is held on this side of the C++/Rust boundary.
 */
class PaintEngine : public QObject {
	Q_OBJECT
public:
	explicit PaintEngine(QObject *parent=nullptr);
	~PaintEngine();

	/// Reset the paint engine to its default state
	void reset();

	/**
	 * @brief Get a reference to the view cache pixmap while makign sure at least the given area has been refreshed
	 */
	const QPixmap &getPixmap(const QRect &refreshArea);

	//! Get a reference to the view cache pixmap while making sure the whole pixmap is refreshed
	const QPixmap &getPixmap() { return getPixmap(QRect{-1, -1, -1, -1}); }

	//! Get the current size of the canvas
	QSize size() const;

	//! Receive and handle messages
	void receiveMessages(bool local, const net::Envelope &msgs);

	//! Clean up dangling state after disconnecting from a remote session
	void cleanup();

	//! Get the color of the background tile
	QColor backgroundColor() const;

	//! Find an unused ID for a new annotation
	uint16_t findAvailableAnnotationId(uint8_t forUser) const;

	//! Get the annotation at the given point
	rustpile::AnnotationAt getAnnotationAt(int x, int y, int expand) const;

	//! Is OpenRaster file format needed to save the canvas losslessly?
	bool needsOpenRaster() const;

	//! Set layerstack rendering mode (normal, solo, onionskin)
	void setViewMode(rustpile::LayerViewMode mode, bool censor);

	//! Set the active view layer (for solo and onionskin modes)
	void setViewLayer(int id);

	//! Set options to use with onion skin layer rendering mode
	void setOnionskinOptions(int skinsBelow, int skinsAbove, bool tint);

	//! Get the raw rustpile paint engine instance
	rustpile::PaintEngine *engine() const { return m_pe; }

signals:
	// Note: these signals are emitted from the paint engine thread
	void areaChanged(const QRect &area);
	void resized(int xoffset, int yoffset, const QSize &oldSize);
	void layersChanged(QVector<LayerListItem> layers);
	void annotationsChanged(rustpile::Annotations *annotations);
	void cursorMoved(uint8_t user, uint16_t layer, int x, int y);

	//! Paint engine has panicked and died
	void enginePanicked();

private:
	rustpile::PaintEngine *m_pe;
	QPixmap m_cache;

	// Callbacks:
	friend void paintEngineAreaChanged(void *pe, rustpile::Rectangle area);
	friend void paintEngineResized(void *pe, int xoffset, int yoffset, rustpile::Size oldSize);
	friend void paintEngineLayersChanged(void *pe, const rustpile::LayerInfo *layers, uintptr_t count);
	friend void paintEngineAnnotationsChanged(void *pe, rustpile::Annotations *annotations);
	friend void paintEngineCursors(void *pe, uint8_t user, uint16_t layer, int32_t x, int32_t y);
};

}

#endif

