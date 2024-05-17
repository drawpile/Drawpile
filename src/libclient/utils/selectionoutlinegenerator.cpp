// SPDX-License-Identifier: GPL-2.0-or-later
// Based on KisOutlineGenerator from Krita
// SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
// SPDX-FileCopyrightText: 2007, 2010 Sven Langkamp <sven.langkamp@gmail.com>
// Outline algorithm based of the limn of fontutils
// SPDX-FileCopyrightText: 1992 Karl Berry <karl@cs.umb.edu>
// SPDX-FileCopyrightText: 1992 Kathryn Hargreaves <letters@cs.umb.edu>
#include "libclient/utils/selectionoutlinegenerator.h"
#include <QPolygon>


namespace {

enum EdgeType {
	TopEdge = 1,
	LeftEdge = 2,
	BottomEdge = 3,
	RightEdge = 0,
	NoEdge = 4
};

bool isTransparent(const uint32_t *bits, int x, int y, int width)
{
	return qAlpha(bits[y * width + x]) == 0;
}

quint8 *pickMark(quint8 *marks, int width, int x, int y)
{
	return marks + width * y + x;
}

bool isOutlineEdge(
	const uint32_t *bits, EdgeType edge, int x, int y, int width, int height)
{
	if(!isTransparent(bits, x, y, width)) {
		switch(edge) {
		case LeftEdge:
			return x == 0 || isTransparent(bits, x - 1, y, width);
		case TopEdge:
			return y == 0 || isTransparent(bits, x, y - 1, width);
		case RightEdge:
			return x == width - 1 || isTransparent(bits, x + 1, y, width);
		case BottomEdge:
			return y == height - 1 || isTransparent(bits, x, y + 1, width);
		case NoEdge:
			break;
		}
	}
	return false;
}

EdgeType nextEdge(EdgeType edge)
{
	return edge == NoEdge ? edge : EdgeType((edge + 1) % 4);
}

#define TRY_PIXEL(deltaRow, deltaCol, test_edge)                               \
	{                                                                          \
		int test_row = *row + deltaRow;                                        \
		int test_col = *col + deltaCol;                                        \
		if((0 <= (test_row) && (test_row) < height && 0 <= (test_col) &&       \
			(test_col) < width) &&                                             \
		   isOutlineEdge(                                                      \
			   bits, test_edge, test_col, test_row, width, height)) {          \
			*row = test_row;                                                   \
			*col = test_col;                                                   \
			*edge = test_edge;                                                 \
			break;                                                             \
		}                                                                      \
	}

void nextOutlineEdge(
	const uint32_t *bits, EdgeType *edge, int *row, int *col, int width,
	int height)
{
	int original_row = *row;
	int original_col = *col;

	switch(*edge) {
	case RightEdge:
		TRY_PIXEL(-1, 0, RightEdge);
		TRY_PIXEL(-1, 1, BottomEdge);
		break;
	case TopEdge:
		TRY_PIXEL(0, -1, TopEdge);
		TRY_PIXEL(-1, -1, RightEdge);
		break;
	case LeftEdge:
		TRY_PIXEL(1, 0, LeftEdge);
		TRY_PIXEL(1, -1, TopEdge);
		break;
	case BottomEdge:
		TRY_PIXEL(0, 1, BottomEdge);
		TRY_PIXEL(1, 1, LeftEdge);
		break;
	default:
		break;
	}

	if(*row == original_row && *col == original_col) {
		*edge = nextEdge(*edge);
	}
}

void appendCoordinate(QPolygon &path, int x, int y, EdgeType edge)
{
	if(edge == TopEdge) {
		++x;
	} else if(edge == BottomEdge) {
		++y;
	} else if(edge == RightEdge) {
		++x;
		++y;
	}
	path.append(QPoint(x, y));
}

QPainterPath generateOutline(
	const QImage &mask, bool simple, int xOffset, int yOffset,
	QAtomicInt &running)
{
	Q_ASSERT(mask.format() == QImage::Format_ARGB32_Premultiplied);
	const uint32_t *bits = reinterpret_cast<const uint32_t *>(mask.constBits());
	int width = mask.width();
	int height = mask.height();

	quint8 *marks = new quint8[width * height];
	memset(marks, 0, width * height);

	QPainterPath paths;
	for(int y = 0; y < height; y++) {
		if(!running) {
			paths.clear();
			break;
		}

		for(int x = 0; x < width; x++) {
			if(isTransparent(bits, x, y, width)) {
				continue;
			}

			const EdgeType initialEdge = TopEdge;

			EdgeType startEdge = initialEdge;
			while(startEdge != NoEdge &&
				  (*pickMark(marks, width, x, y) & (1 << startEdge) ||
				   !isOutlineEdge(bits, startEdge, x, y, width, height))) {

				startEdge = nextEdge(startEdge);
				if(startEdge == initialEdge)
					startEdge = NoEdge;
			}

			if(startEdge != NoEdge) {
				QPolygon path;
				const bool clockwise = startEdge == BottomEdge;

				int row = y, col = x;
				EdgeType currentEdge = startEdge;
				EdgeType lastEdge = NoEdge;

				if(currentEdge == BottomEdge) {
					appendCoordinate(
						path, col + xOffset, row + yOffset, currentEdge);
					lastEdge = BottomEdge;
				}

				while(true) {
					*pickMark(marks, width, col, row) |= 1 << currentEdge;
					nextOutlineEdge(
						bits, &currentEdge, &row, &col, width, height);

					// While following a straight line no points need to be
					// added
					if(lastEdge != currentEdge) {
						appendCoordinate(
							path, col + xOffset, row + yOffset, currentEdge);
						lastEdge = currentEdge;
					}

					if(row == y && col == x && currentEdge == startEdge) {
						if(startEdge != BottomEdge) {
							// add last point of the polygon
							appendCoordinate(
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

	delete[] marks;
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
