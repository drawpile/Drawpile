// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_CURSORS
#define LIBCLIENT_UTILS_CURSORS
#include <QCursor>

namespace utils {

class Cursors {
public:
	static void init()
	{
		if(!instance) {
			instance = new Cursors;
		}
	}

	static const QCursor &arrow() { return get()->m_arrow; }
	static const QCursor &bucket() { return get()->m_bucket; }
	static const QCursor &bucketCheck() { return get()->m_bucketCheck; }
	static const QCursor &check() { return get()->m_check; }
	static const QCursor &colorPick() { return get()->m_colorPick; }
	static const QCursor &curve() { return get()->m_curve; }
	static const QCursor &dot() { return get()->m_dot; }
	static const QCursor &ellipse() { return get()->m_ellipse; }
	static const QCursor &eraser() { return get()->m_eraser; }
	static const QCursor &layerPick() { return get()->m_layerPick; }
	static const QCursor &line() { return get()->m_line; }
	static const QCursor &magicWand() { return get()->m_magicWand; }
	static const QCursor &magicWandExclude()
	{
		return get()->m_magicWandExclude;
	}
	static const QCursor &magicWandIntersect()
	{
		return get()->m_magicWandIntersect;
	}
	static const QCursor &magicWandUnite() { return get()->m_magicWandUnite; }
	static const QCursor &move() { return get()->m_move; }
	static const QCursor &moveMask() { return get()->m_moveMask; }
	static const QCursor &rectangle() { return get()->m_rectangle; }
	static const QCursor &rotate() { return get()->m_rotate; }
	static const QCursor &rotateDiscrete() { return get()->m_rotateDiscrete; }
	static const QCursor &selectLasso() { return get()->m_selectLasso; }
	static const QCursor &selectLassoExclude()
	{
		return get()->m_selectLassoExclude;
	}
	static const QCursor &selectLassoIntersect()
	{
		return get()->m_selectLassoIntersect;
	}
	static const QCursor &selectLassoUnite()
	{
		return get()->m_selectLassoUnite;
	}
	static const QCursor &selectRectangle() { return get()->m_selectRectangle; }
	static const QCursor &selectRectangleExclude()
	{
		return get()->m_selectRectangleExclude;
	}
	static const QCursor &selectRectangleIntersect()
	{
		return get()->m_selectRectangleIntersect;
	}
	static const QCursor &selectRectangleUnite()
	{
		return get()->m_selectRectangleUnite;
	}
	static const QCursor &shearDiagB() { return get()->m_shearDiagB; }
	static const QCursor &shearDiagF() { return get()->m_shearDiagF; }
	static const QCursor &shearH() { return get()->m_shearH; }
	static const QCursor &shearV() { return get()->m_shearV; }
	static const QCursor &text() { return get()->m_text; }
	static const QCursor &triangleLeft() { return get()->m_triangleLeft; }
	static const QCursor &triangleRight() { return get()->m_triangleRight; }
	static const QCursor &zoom() { return get()->m_zoom; }

private:
	Cursors();

	static const Cursors *get()
	{
		Q_ASSERT(instance);
		return instance;
	}

	static QCursor generateDotCursor();

	static Cursors *instance;
	const QCursor m_arrow;
	const QCursor m_bucket;
	const QCursor m_bucketCheck;
	const QCursor m_check;
	const QCursor m_colorPick;
	const QCursor m_curve;
	const QCursor m_dot;
	const QCursor m_ellipse;
	const QCursor m_eraser;
	const QCursor m_layerPick;
	const QCursor m_line;
	const QCursor m_magicWand;
	const QCursor m_magicWandExclude;
	const QCursor m_magicWandIntersect;
	const QCursor m_magicWandUnite;
	const QCursor m_move;
	const QCursor m_moveMask;
	const QCursor m_rectangle;
	const QCursor m_rotate;
	const QCursor m_rotateDiscrete;
	const QCursor m_selectLasso;
	const QCursor m_selectLassoExclude;
	const QCursor m_selectLassoIntersect;
	const QCursor m_selectLassoUnite;
	const QCursor m_selectRectangle;
	const QCursor m_selectRectangleExclude;
	const QCursor m_selectRectangleIntersect;
	const QCursor m_selectRectangleUnite;
	const QCursor m_shearDiagB;
	const QCursor m_shearDiagF;
	const QCursor m_shearH;
	const QCursor m_shearV;
	const QCursor m_text;
	const QCursor m_triangleLeft;
	const QCursor m_triangleRight;
	const QCursor m_zoom;
};

}

#endif
