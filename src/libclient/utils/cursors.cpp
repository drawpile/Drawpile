// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/cursors.h"
#include <QPainter>
#include <QPixmap>

namespace utils {

Cursors *Cursors::instance;

Cursors::Cursors()
	: m_arrow(QPixmap(":cursors/arrow.png"), 1, 1)
	, m_bucket(QPixmap(QStringLiteral(":cursors/bucket.png")), 2, 29)
	, m_bucketCheck(QPixmap(QStringLiteral(":cursors/bucketcheck.png")), 2, 29)
	, m_check(QCursor(QPixmap(QStringLiteral(":/cursors/check.png")), 16, 16))
	, m_colorPick(
		  QCursor(QPixmap(QStringLiteral(":/cursors/colorpicker.png")), 2, 29))
	, m_curve(QPixmap(":cursors/curve.png"), 2, 2)
	, m_dot(generateDotCursor())
	, m_ellipse(QPixmap(":cursors/ellipse.png"), 2, 2)
	, m_eraser(QCursor(QPixmap(QStringLiteral(":/cursors/eraser.png")), 2, 2))
	, m_gradient(
		  QCursor(QPixmap(QStringLiteral(":/cursors/gradient.png")), 2, 2))
	, m_layerPick(
		  QCursor(QPixmap(QStringLiteral(":/cursors/layerpicker.png")), 2, 29))
	, m_line(QPixmap(":cursors/line.png"), 2, 2)
	, m_magicWand(QPixmap(":cursors/magicwand.png"), 2, 2)
	, m_magicWandExclude(QPixmap(":cursors/magicwand-exclude.png"), 2, 2)
	, m_magicWandIntersect(QPixmap(":cursors/magicwand-intersect.png"), 2, 2)
	, m_magicWandUnite(QPixmap(":cursors/magicwand-unite.png"), 2, 2)
	, m_move(QPixmap(QStringLiteral(":/cursors/move.png")), 12, 12)
	, m_moveMask(QPixmap(QStringLiteral(":/cursors/move-mask.png")), 7, 7)
	, m_rectangle(QPixmap(":cursors/rectangle.png"), 2, 2)
	, m_rotate(QCursor(QPixmap(QStringLiteral(":/cursors/rotate.png")), 16, 16))
	, m_rotateDiscrete(QCursor(
		  QPixmap(QStringLiteral(":/cursors/rotate-discrete.png")), 16, 16))
	, m_selectLasso(QPixmap(":cursors/select-lasso.png"), 2, 29)
	, m_selectLassoExclude(QPixmap(":cursors/select-lasso-exclude.png"), 2, 29)
	, m_selectLassoIntersect(
		  QPixmap(":cursors/select-lasso-intersect.png"), 2, 29)
	, m_selectLassoUnite(QPixmap(":cursors/select-lasso-unite.png"), 2, 29)
	, m_selectRectangle(QPixmap(":cursors/select-rectangle.png"), 2, 2)
	, m_selectRectangleExclude(
		  QPixmap(":cursors/select-rectangle-exclude.png"), 2, 2)
	, m_selectRectangleIntersect(
		  QPixmap(":cursors/select-rectangle-intersect.png"), 2, 2)
	, m_selectRectangleUnite(
		  QPixmap(":cursors/select-rectangle-unite.png"), 2, 2)
	, m_shearDiagB(QPixmap(QStringLiteral(":/cursors/shear-bdiag.png")), 13, 13)
	, m_shearDiagF(QPixmap(QStringLiteral(":/cursors/shear-fdiag.png")), 13, 13)
	, m_shearH(QPixmap(QStringLiteral(":/cursors/shear-h.png")), 12, 12)
	, m_shearV(QPixmap(QStringLiteral(":/cursors/shear-v.png")), 12, 12)
	, m_text(QPixmap(":cursors/text.png"), 2, 2)
	, m_triangleLeft(QCursor(
		  QPixmap(QStringLiteral(":/cursors/triangle-left.png")), 14, 14))
	, m_triangleRight(QCursor(
		  QPixmap(QStringLiteral(":/cursors/triangle-right.png")), 14, 14))
	, m_zoom(QCursor(QPixmap(QStringLiteral(":/cursors/zoom.png")), 8, 8))
{
}

QCursor Cursors::generateDotCursor()
{
	QPixmap dot(8, 8);
	dot.fill(Qt::transparent);
	QPainter p(&dot);
	p.setPen(Qt::white);
	p.drawPoint(0, 0);
	return QCursor(dot, 0, 0);
}

}
