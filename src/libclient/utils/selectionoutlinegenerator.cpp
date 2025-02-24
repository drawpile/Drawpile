// SPDX-License-Identifier: GPL-2.0-or-later
// Based on KisOutlineGenerator from Krita
// SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
// SPDX-FileCopyrightText: 2007, 2010 Sven Langkamp <sven.langkamp@gmail.com>
// Outline algorithm based of the limn of fontutils
// SPDX-FileCopyrightText: 1992 Karl Berry <karl@cs.umb.edu>
// SPDX-FileCopyrightText: 1992 Kathryn Hargreaves <letters@cs.umb.edu>
#include "libclient/utils/selectionoutlinegenerator.h"
#include "libshared/util/qtcompat.h"
#include <QPolygon>
#include <QScopedPointer>

namespace {

enum EdgeType {
	TopEdge = 1,
	LeftEdge = 2,
	BottomEdge = 3,
	RightEdge = 0,
	NoEdge = 4
};

EdgeType nextEdge(EdgeType edge)
{
	return edge == NoEdge ? edge : EdgeType((edge + 1) % 4);
}

#define TRY_PIXEL(deltaRow, deltaCol, test_edge)                               \
	{                                                                          \
		long long test_row = *row + deltaRow;                                  \
		long long test_col = *col + deltaCol;                                  \
		if((0LL <= (test_row) && (test_row) < m_scaledHeight &&                \
			0LL <= (test_col) && (test_col) < m_scaledWidth) &&                \
		   isOutlineEdge(test_edge, test_col, test_row)) {                     \
			*row = test_row;                                                   \
			*col = test_col;                                                   \
			*edge = test_edge;                                                 \
			break;                                                             \
		}                                                                      \
	}

class OutlineMap {
	COMPAT_DISABLE_COPY_MOVE(OutlineMap)
public:
	static constexpr long long DIMENSION_LIMIT = 2048;

	OutlineMap(
		const QImage &mask, long long scaledWidth, long long scaledHeight)
		: m_actualWidth(mask.width())
		, m_actualHeight(mask.height())
		, m_scaledWidth(scaledWidth)
		, m_scaledHeight(scaledHeight)
		, m_bits(reinterpret_cast<const uint32_t *>(mask.constBits()))
	{
	}

	virtual ~OutlineMap() { delete[] m_marks; }

	virtual uint32_t at(long long x, long long y) const = 0;
	virtual bool isFullyOpaque() const = 0;
	virtual void
	appendToPath(QPolygon &path, long long x, long long y) const = 0;

	long long actualWidth() const { return m_actualWidth; }
	long long actualHeight() const { return m_actualHeight; }
	long long scaledWidth() const { return m_scaledWidth; }
	long long scaledHeight() const { return m_scaledHeight; }

	void allocateMarks()
	{
		Q_ASSERT(!m_marks);
		long long size = m_scaledWidth * m_scaledHeight;
		m_marks = new uint8_t[size];
		memset(m_marks, 0, size);
	}

	uint8_t *pickMark(long long x, long long y)
	{
		Q_ASSERT(x >= 0);
		Q_ASSERT(y >= 0);
		Q_ASSERT(x < m_scaledWidth);
		Q_ASSERT(y < m_scaledHeight);
		return m_marks + y * m_scaledWidth + x;
	}

	bool isTransparent(long long x, long long y) const
	{
		return qAlpha(at(x, y)) == 0LL;
	}

	bool isOutlineEdge(EdgeType edge, long long x, long long y) const
	{
		if(!isTransparent(x, y)) {
			switch(edge) {
			case LeftEdge:
				return x == 0LL || isTransparent(x - 1LL, y);
			case TopEdge:
				return y == 0LL || isTransparent(x, y - 1LL);
			case RightEdge:
				return x == m_scaledWidth - 1LL || isTransparent(x + 1LL, y);
			case BottomEdge:
				return y == m_scaledHeight - 1LL || isTransparent(x, y + 1LL);
			case NoEdge:
				break;
			}
		}
		return false;
	}

	void nextOutlineEdge(EdgeType *edge, long long *row, long long *col) const
	{
		long long originalRow = *row;
		long long originalCol = *col;

		switch(*edge) {
		case RightEdge:
			TRY_PIXEL(-1LL, 0LL, RightEdge);
			TRY_PIXEL(-1LL, 1LL, BottomEdge);
			break;
		case TopEdge:
			TRY_PIXEL(0LL, -1LL, TopEdge);
			TRY_PIXEL(-1LL, -1LL, RightEdge);
			break;
		case LeftEdge:
			TRY_PIXEL(1LL, 0LL, LeftEdge);
			TRY_PIXEL(1LL, -1LL, TopEdge);
			break;
		case BottomEdge:
			TRY_PIXEL(0LL, 1LL, BottomEdge);
			TRY_PIXEL(1LL, 1LL, LeftEdge);
			break;
		default:
			break;
		}

		if(*row == originalRow && *col == originalCol) {
			*edge = nextEdge(*edge);
		}
	}

	void appendCoordinate(
		QPolygon &path, long long x, long long y, EdgeType edge) const
	{
		if(edge == TopEdge) {
			++x;
		} else if(edge == BottomEdge) {
			++y;
		} else if(edge == RightEdge) {
			++x;
			++y;
		}
		appendToPath(path, x, y);
	}

protected:
	const long long m_actualWidth;
	const long long m_actualHeight;
	const long long m_scaledWidth;
	const long long m_scaledHeight;
	const uint32_t *const m_bits;

private:
	uint8_t *m_marks = nullptr;
};

class PlainOutlineMap final : public OutlineMap {
	COMPAT_DISABLE_COPY_MOVE(PlainOutlineMap)
public:
	PlainOutlineMap(const QImage &mask)
		: OutlineMap(mask, mask.width(), mask.height())
	{
	}

	uint32_t at(long long x, long long y) const override
	{
		return m_bits[y * m_actualWidth + x];
	}

	bool isFullyOpaque() const override
	{
		long long size = m_actualWidth * m_actualHeight;
		for(long long i = 0LL; i < size; ++i) {
			if(qAlpha(m_bits[i]) == 0u) {
				return false;
			}
		}
		return true;
	}

	void appendToPath(QPolygon &path, long long x, long long y) const override
	{
		path.append(QPoint(x, y));
	}
};

class ScaledOutlineMap final : public OutlineMap {
	COMPAT_DISABLE_COPY_MOVE(ScaledOutlineMap)
public:
	ScaledOutlineMap(const QImage &mask)
		: OutlineMap(
			  mask, clampDimension(mask.width()), clampDimension(mask.height()))
		, m_ratioX(qreal(m_actualWidth) / qreal(m_scaledWidth))
		, m_ratioY(qreal(m_actualHeight) / qreal(m_scaledHeight))
	{
	}

	uint32_t at(long long x, long long y) const override
	{
		long long actualX = toActualX(x);
		Q_ASSERT(actualX >= 0LL);
		Q_ASSERT(actualX < m_actualWidth);
		long long actualY = toActualY(y);
		Q_ASSERT(actualY >= 0LL);
		Q_ASSERT(actualY < m_actualHeight);
		return m_bits[actualY * m_actualWidth + actualX];
	}

	bool isFullyOpaque() const override
	{
		for(long long y = 0LL; y < m_scaledHeight; ++y) {
			for(long long x = 0LL; x < m_scaledWidth; ++x) {
				if(isTransparent(x, y)) {
					return false;
				}
			}
		}
		return true;
	}

	void appendToPath(QPolygon &path, long long x, long long y) const override
	{
		path.append(QPoint(toActualX(x), toActualY(y)));
	}

private:
	static long long clampDimension(long long dimension)
	{
		return dimension < DIMENSION_LIMIT ? dimension : DIMENSION_LIMIT;
	}

	long long toActualX(long long scaledX) const
	{
		return qRound64(qreal(scaledX) * m_ratioX);
	}

	long long toActualY(long long scaledY) const
	{
		return qRound64(qreal(scaledY) * m_ratioY);
	}

	const qreal m_ratioX;
	const qreal m_ratioY;
};

OutlineMap *makeOutlineSource(const QImage &mask)
{
	if(mask.width() < OutlineMap::DIMENSION_LIMIT &&
	   mask.height() < OutlineMap::DIMENSION_LIMIT) {
		return new PlainOutlineMap(mask);
	} else {
		return new ScaledOutlineMap(mask);
	}
}

QPainterPath generateOutline(
	const QImage &mask, bool simple, int xOffset, int yOffset,
	QAtomicInt &running)
{
	Q_ASSERT(mask.format() == QImage::Format_ARGB32_Premultiplied);
	QScopedPointer<OutlineMap> o(makeOutlineSource(mask));

	QPainterPath paths;
	if(o->isFullyOpaque()) {
		paths.addRect(xOffset, yOffset, o->actualWidth(), o->actualHeight());
		return paths;
	}

	o->allocateMarks();

	long long scaledWidth = o->scaledWidth();
	long long scaledHeight = o->scaledHeight();
	for(long long y = 0; y < scaledHeight; y++) {
		if(!running) {
			paths.clear();
			break;
		}

		for(long long x = 0; x < scaledWidth; x++) {
			if(o->isTransparent(x, y)) {
				continue;
			}

			const EdgeType initialEdge = TopEdge;

			EdgeType startEdge = initialEdge;
			while(startEdge != NoEdge &&
				  (*o->pickMark(x, y) & (1 << startEdge) ||
				   !o->isOutlineEdge(startEdge, x, y))) {

				startEdge = nextEdge(startEdge);
				if(startEdge == initialEdge)
					startEdge = NoEdge;
			}

			if(startEdge != NoEdge) {
				QPolygon path;
				const bool clockwise = startEdge == BottomEdge;

				long long row = y, col = x;
				EdgeType currentEdge = startEdge;
				EdgeType lastEdge = NoEdge;

				if(currentEdge == BottomEdge) {
					o->appendCoordinate(
						path, col + xOffset, row + yOffset, currentEdge);
					lastEdge = BottomEdge;
				}

				while(true) {
					*o->pickMark(col, row) |= 1 << currentEdge;
					o->nextOutlineEdge(&currentEdge, &row, &col);

					// While following a straight line no points need to be
					// added
					if(lastEdge != currentEdge) {
						o->appendCoordinate(
							path, col + xOffset, row + yOffset, currentEdge);
						lastEdge = currentEdge;
					}

					if(row == y && col == x && currentEdge == startEdge) {
						if(startEdge != BottomEdge) {
							// add last point of the polygon
							o->appendCoordinate(
								path, col + xOffset, row + yOffset, NoEdge);
						}
						break;
					}
				}

				if(!simple || !clockwise) {
					paths.addPolygon(path);
				}
			}
		}
	}

	return paths;
}

}


SelectionOutlineGenerator::SelectionOutlineGenerator(
	unsigned int executionId, const QImage &mask, bool simple, int xOffset,
	int yOffset, QObject *parent)
	: QObject(parent)
	, m_executionId(executionId)
	, m_mask(mask)
	, m_simple(simple)
	, m_xOffset(xOffset)
	, m_yOffset(yOffset)
{
	qRegisterMetaType<SelectionOutlinePath>();
}

void SelectionOutlineGenerator::run()
{
	if(m_running) {
		SelectionOutlinePath path = {
			generateOutline(m_mask, m_simple, m_xOffset, m_yOffset, m_running)};
		if(m_running) {
			emit outlineGenerated(m_executionId, path);
		}
	}
}

void SelectionOutlineGenerator::cancel()
{
	m_running = 0;
}
