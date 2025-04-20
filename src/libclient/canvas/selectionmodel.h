// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_SELECTIONMODEL_H
#define LIBCLIENT_CANVAS_SELECTIONMODEL_H
#include "libclient/drawdance/layercontent.h"
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSharedPointer>

struct DP_Mutex;

namespace drawdance {
class SelectionSet;
}

namespace canvas {

class CanvasModel;
class SelectionModel;

class SelectionMask final {
public:
	SelectionMask(
		SelectionModel *model, const drawdance::LayerContent &content,
		const QRect &bounds);

	const drawdance::LayerContent &content() const { return m_content; }
	const QRect &bounds() const { return m_bounds; }
	const QImage &image() const;

private:
	const QPointer<SelectionModel> m_model;
	const drawdance::LayerContent m_content;
	const QRect m_bounds;
	mutable QImage m_image;
	mutable bool m_maskValid = false;
};

class SelectionModel final : public QObject {
	Q_OBJECT
public:
	explicit SelectionModel(QObject *parent);
	~SelectionModel();

	void setLocalUserId(uint8_t localUserId) { m_localUserId = localUserId; }

	bool isValid() const { return !m_mask.isNull(); }

	const QSharedPointer<SelectionMask> &mask() const { return m_mask; }
	QRect bounds() const { return m_mask ? m_mask->bounds() : QRect(); }
	QImage image() const { return m_mask ? m_mask->image() : QImage(); }

	void reifyImageMask(
		const QRect &bounds, const drawdance::LayerContent &content,
		QImage &outMask, bool &inOutMaskValid);

public slots:
	void setSelections(const drawdance::SelectionSet &ss);

signals:
	void selectionChanged(const QSharedPointer<SelectionMask> &mask);

private:
	uint8_t m_localUserId = 0;
	QSharedPointer<SelectionMask> m_mask;
	DP_Mutex *m_mutex;
};

}

#endif
