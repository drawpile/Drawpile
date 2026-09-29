// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_LIQUIFY_H
#define LIBCLIENT_TOOLS_LIQUIFY_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/image/kis_liquify_transform_worker.h"
#include "libclient/tools/tool.h"

namespace canvas {
class TransformModel;
}

namespace drawdance {
class Liquify;
}

namespace tools {

class LiquifyTool final : public Tool {
public:
	enum class Operation {
		Move,
		Scale,
		Rotate,
		Smoothe,
		Undo,
		Last = Undo,
	};

	struct Properties {
		Operation operation = Operation::Move;
		qreal size = 60.0;
		qreal amount = 0.05;
		qreal spacing = 0.2;
		bool sizePressure = false;
		bool amountPressure = false;
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

	void beginTemporary(Tool::Type toolToReturnTo);
	void clearTemporary();

	void setProperties(const Properties &props) { m_props = props; }

private:
	static constexpr int MAX_STATE_STACK_DEPTH = 50;
	static constexpr float SMOOTHE_KERNEL_RADIUS = 1.5f;

	bool isLiquifyActive() const;
	canvas::TransformModel *getActiveLiquifyModel() const;
	canvas::TransformModel *tryBeginLiquify();
	void endLiquify(canvas::TransformModel *transform, bool applied);

	void processDabs();
	void applyDabs(drawdance::Liquify &liquify);

	qreal effectiveAmount(qreal amount) const
	{
		if(m_reverse) {
			return -amount;
		} else {
			return amount;
		}
	}

	void pushState(canvas::TransformModel *transform);

	void returnToPreviousTool();

	Properties m_props;
	drawdance::LiquifyEngine m_liquifyEngine;
	canvas::Point m_firstPoint;
	QVector<KisLiquifyTransformWorker::State> m_stateStack;
	qreal m_zoom = 1.0;
	Operation m_operation = Operation::Move;
	Tool::Type m_toolToReturnTo = Tool::Type::_LASTTOOL;
	int m_stateStackTop = -1;
	bool m_reverse = false;
	bool m_drawing = false;
	bool m_strokeStarted = false;
};

}

#endif
