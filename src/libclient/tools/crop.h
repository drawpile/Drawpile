// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_CROP_H
#define LIBCLIENT_TOOLS_CROP_H
#include "libclient/tools/tool.h"

namespace tools {

class CropTool final : public Tool {
public:
	enum class Handle {
		Invalid = -1,
		None = 0,
		TopLeft,
		Top,
		TopRight,
		Right,
		BottomRight,
		Bottom,
		BottomLeft,
		Left,
		TopEdge,
		RightEdge,
		BottomEdge,
		LeftEdge,
		Outside,
		Inside,
	};

	struct Properties {
		bool grow = false;
	};

	CropTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	void redoMultipart() override;
	bool isMultipart() const override;
	void finish() override;

	void setProperties(const Properties &props) { m_props = props; }
	void setCropArea(QRect cropArea);

private:
	QPoint floorPoint(QPointF point);
	QRect getCurrentRect() const;

	Properties m_props;
	QPoint m_start;
	QPoint m_end;
	bool m_active = false;
	bool m_drawing = false;
};

}

#endif
