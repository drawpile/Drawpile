// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_TOOL_H
#define LIBCLIENT_TOOLS_TOOL_H
#include "libclient/canvas/point.h"
#include "libclient/tools/enums.h"
#include "libclient/tools/toolstate.h"
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
	};

	struct EndParams {
		bool constrain;
		bool center;
	};

	Tool(
		ToolController &owner, Type type, const QCursor &cursor,
		bool allowColorPick, bool allowToolAdjust, bool allowRightClick,
		bool fractional, bool supportsPressure, bool ignoresSelections)
		: m_owner{owner}
		, m_type{type}
		, m_cursor{cursor}
		, m_allowColorPick{allowColorPick}
		, m_allowToolAdjust{allowToolAdjust}
		, m_handlesRightClick{allowRightClick}
		, m_fractional(fractional)
		, m_supportsPressure(supportsPressure)
		, m_ignoresSelections(ignoresSelections)
	{
	}

	virtual ~Tool() {}

	Tool(const Tool &) = delete;
	Tool(Tool &&) = delete;
	Tool &operator=(const Tool &) = delete;
	Tool &operator=(Tool &&) = delete;

	Type type() const { return m_type; }
	const QCursor &cursor() const { return m_cursor; }
	bool allowColorPick() const { return m_allowColorPick; }
	bool allowToolAdjust() const { return m_allowToolAdjust; }
	bool handlesRightClick() const { return m_handlesRightClick; }
	bool isFractional() const { return m_fractional; }
	bool supportsPressure() const { return m_supportsPressure; }
	bool ignoresSelections() const { return m_ignoresSelections; }

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

protected:
	bool isActiveTool() const;
	void setHandlesRightClick(bool handlesRightClick);
	void setCursor(const QCursor &cursor);
	void requestToolNotice(const QString &text);

	ToolController &m_owner;

private:
	const Type m_type;
	QCursor m_cursor; // May change during tool operation.
	const bool m_allowColorPick;
	const bool m_allowToolAdjust;
	bool m_handlesRightClick; // May change during tool operation.
	const bool m_fractional;
	const bool m_supportsPressure;
	const bool m_ignoresSelections;
};

}

Q_DECLARE_METATYPE(tools::Tool::Type)

#endif
