// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_ANNOTATION_H
#define TOOLS_ANNOTATION_H
#include "libclient/tools/tool.h"
#include <QRect>
#include <QSet>

namespace tools {

/**
 * @brief Annotation tool
 */
class Annotation final : public Tool {
public:
	Annotation(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

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

	void deselectAnnotation();
	Handle handleAt(const QRect &rect, const QPoint &p, int handleSize);

	int getAvailableAnnotationId();
	int searchAvailableAnnotationId(
		const QSet<int> &takenIds, unsigned int contextId);

	/// ID of the currently selected annotation
	int m_selectedId = 0;

	/// Are we currently creating a new annotation?
	bool m_isNew = true;

	/// Drag start and last point
	QPointF m_p1, m_p2;

	/// Grabbed handle
	Handle m_handle = Handle::Outside;

	/// The shape of the annotation being edited
	QRect m_shape;
};

}

#endif
