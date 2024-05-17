// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_SELECTIONMODEL_H
#define LIBCLIENT_CANVAS_SELECTIONMODEL_H
#include "libclient/drawdance/layercontent.h"
#include "libclient/utils/transformquad.h"
#include <QImage>
#include <QObject>
#include <QRect>

namespace drawdance {
class SelectionSet;
}

namespace canvas {

class CanvasModel;

class SelectionModel final : public QObject {
	Q_OBJECT
public:
	explicit SelectionModel(QObject *parent);

	void setLocalUserId(uint8_t localUserId) { m_localUserId = localUserId; }

	bool isValid() const { return !m_content.isNull(); }

	const QRect &bounds() const { return m_bounds; }
	const QImage &mask() const { return m_mask; }

public slots:
	void setSelections(const drawdance::SelectionSet &ss);

signals:
	void selectionChanged(bool valid, const QRect &bounds, const QImage &mask);

private:
	uint8_t m_localUserId = 0;
	drawdance::LayerContent m_content = drawdance::LayerContent::null();
	QRect m_bounds;
	QImage m_mask;
};

}

#endif
