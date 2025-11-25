// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_VIEW_ZOOM_H
#define LIBCLIENT_VIEW_ZOOM_H
#include <QVector>

namespace view {

qreal getZoomMin();
qreal getZoomMax();
qreal getZoomSoftMin();
qreal getZoomSoftMax();
const QVector<qreal> &getZoomLevels();
void setZoomLevelsCanvasImplementation(int canvasImplementation);

}

#endif
