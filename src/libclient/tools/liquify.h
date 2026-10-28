// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_LIQUIFY_H
#define LIBCLIENT_TOOLS_LIQUIFY_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/tools/tool.h"

namespace canvas {
class TransformModel;
}

namespace drawdance {
class LiquifyWorker;
}

namespace tools {

class LiquifyTool final : public Tool {
public:
	enum class Operation {
		Move,
		Scale,
		Rotate,
		Smoothe,
		Erase,
		Last = Erase,
	};

	struct Properties {
		Operation operation = Operation::Move;
		qreal size = 60.0;
		qreal amount = 0.05;
		qreal hardness = 0.5;
		qreal spacing = 0.2;
		bool sizePressure = false;
		bool amountPressure = false;
		bool hardnessPressure = false;
		bool reverse = false;
	};

	LiquifyTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	void redoMultipart() override;
	bool isMultipart() const override;

	void offsetActiveTool(int x, int y) override;

	void beginTemporary(Tool::Type toolToReturnTo);
	void clearTemporary();

	bool handleDeselect(bool finish);

	void stamp() override;

	void setProperties(const Properties &props) { m_props = props; }
	void updateDisplacement(float displacement);
	void applyDisplacement();

private:
	static constexpr int UNDO_DEPTH = 100;
	static constexpr float SMOOTHE_KERNEL_RADIUS = 1.5f;
	static constexpr unsigned int FLAG_CONSTRAIN = (1u << 0u);
	static constexpr unsigned int FLAG_CENTER = (1u << 1u);

	bool isLiquifyActive() const;
	canvas::TransformModel *getActiveLiquifyModel() const;
	drawdance::LiquifyWorker *getActiveLiquifyWorker() const;
	QPointF getLiquifyOffset() const;
	canvas::TransformModel *tryBeginLiquify();
	void endLiquify(canvas::TransformModel *transform, bool applied);

	void setLiquifyWorkerOperation(drawdance::LiquifyWorker *liquifyWorker);

	void apply(bool stamp);

	void returnToPreviousTool();

	Properties m_props;
	canvas::Point m_firstPoint;
	qreal m_zoom = 1.0;
	Tool::Type m_toolToReturnTo = Tool::Type::_LASTTOOL;
	bool m_drawing = false;
	bool m_strokeStarted = false;
};

}

#endif
