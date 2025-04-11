// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_TOOL_H
#define LIBCLIENT_TOOLS_TOOL_H
#include "libclient/canvas/point.h"
#include "libclient/tools/enums.h"
#include <QCursor>
#include <QMetaType>

/**
 * @brief Tools
 *
 * Tools translate commands from the local user into messages that
 * can be sent over the network.
 * Read-only tools can access the canvas directly.
 */
namespace tools {

class ToolController;

/**
 * @brief Base class for all tools
 * Tool classes interpret mouse/pen commands into editing actions.
 */
class Tool {
public:
	enum Type {
		FREEHAND,
		ERASER,
		LINE,
		RECTANGLE,
		ELLIPSE,
		BEZIER,
		FLOODFILL,
		ANNOTATION,
		PICKER,
		LASERPOINTER,
		SELECTION,
		POLYGONSELECTION,
		MAGICWAND,
		TRANSFORM,
		PAN,
		ZOOM,
		INSPECTOR,
		_LASTTOOL,
	};

	struct BeginParams {
		canvas::Point point;
		QPointF viewPos;
		qreal angle;
		qreal zoom;
		DeviceType deviceType;
		bool mirror;
		bool flip;
		bool right;
		bool constrain;
		bool center;
	};

	struct MotionParams {
		canvas::Point point;
		QPointF viewPos;
		bool constrain;
		bool center;
	};

	struct ModifyParams {
		bool constrain;
		bool center;
	};

	struct HoverParams {
		QPointF point;
		qreal angle;
		qreal zoom;
		bool mirror;
		bool flip;
		bool constrain;
		bool center;
	};

	struct EndParams {
		bool constrain;
		bool center;
	};

	Tool(
		ToolController &owner, Type type, const QCursor &cursor,
		Capabilities capabilities)
		: m_owner(owner)
		, m_type(type)
		, m_cursor(cursor)
		, m_capabilities(capabilities)
	{
	}

	virtual ~Tool() {}

	Tool(const Tool &) = delete;
	Tool(Tool &&) = delete;
	Tool &operator=(const Tool &) = delete;
	Tool &operator=(Tool &&) = delete;

	Type type() const { return m_type; }
	const QCursor &cursor() const { return m_cursor; }
	Capabilities capabilities() const { return m_capabilities; }

	virtual void begin(const BeginParams &params) = 0;
	virtual void motion(const MotionParams &params) = 0;
	virtual void modify(const ModifyParams &params) { Q_UNUSED(params); }
	virtual void hover(const HoverParams &params) { Q_UNUSED(params); }
	virtual void end(const EndParams &params) = 0;

	//! Finish and commit a multipart stroke
	virtual void finishMultipart() {}

	//! Cancel the current multipart stroke (if any)
	virtual void cancelMultipart() {}

	//! Undo the latest step of a multipart stroke. Undoing the first part
	//! should cancel the stroke
	virtual void undoMultipart() {}

	//! Redo the previously undone step of a multipart stroke, if any.
	virtual void redoMultipart() {}

	//! Is there a multipart stroke in progress at the moment?
	virtual bool isMultipart() const { return false; }

	//! Called before destruction, get rid of whatever necessary.
	virtual void dispose() {}

	//! Called before commands are sent. Used to flush fill previews.
	virtual void flushPreviewedActions() {}

	//! Does this tool update the color swatch when used?
	virtual bool usesBrushColor() const { return false; }

	//! Add an offset to this tool's current position (if active)
	virtual void offsetActiveTool(int x, int y)
	{
		Q_UNUSED(x)
		Q_UNUSED(y) /* most tools don't need to do anything here */
	}

	//! Active layer id changed, most tools don't need to do anything here
	virtual void setActiveLayer(int layerId) { Q_UNUSED(layerId); }

	//! Current foreground color changed
	virtual void setForegroundColor(const QColor &color) { Q_UNUSED(color); }

	virtual void setBrushSizeLimit(int limit) { Q_UNUSED(limit); }

protected:
	bool isActiveTool() const;
	void setCapability(Capability capability, bool enabled);
	void setCursor(const QCursor &cursor);
	void requestToolNotice(const QString &text);

	ToolController &m_owner;

private:
	const Type m_type;
	QCursor m_cursor;
	Capabilities m_capabilities;
};

}

Q_DECLARE_METATYPE(tools::Tool::Type)

#endif
