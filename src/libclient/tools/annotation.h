// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_ANNOTATION_H
#define TOOLS_ANNOTATION_H
#include "libclient/tools/tool.h"
#include <QRect>

namespace tools {

/**
 * @brief Annotation tool
 */
class Annotation final : public Tool {
public:
	Annotation(ToolController &owner);

	void begin(
		const canvas::Point &point, bool right, float zoom,
		const QPointF &viewPos) override;

	void motion(
		const canvas::Point &point, bool constrain, bool center,
		const QPointF &viewPos) override;

	void end() override;

private:
	/// Where the annotation was grabbed
	enum class Handle {
		Outside,
		Inside,
		TopLeft,
		TopRight,
		BottomRight,
		BottomLeft,
		Top,
		Right,
		Bottom,
		Left
	};

	/// ID of the currently selected annotation
	uint16_t m_selectedId;

	/// Are we currently creating a new annotation?
	bool m_isNew;

	/// Drag start and last point
	QPointF m_p1, m_p2;

	/// Grabbed handle
	Handle m_handle;

	/// The shape of the annotation being edited
	QRect m_shape;

	Handle handleAt(const QRect &rect, const QPoint &p, int handleSize);
};

}

#endif
