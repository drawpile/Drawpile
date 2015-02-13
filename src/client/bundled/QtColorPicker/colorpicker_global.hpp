#ifndef QT_COLOR_PICKER_GLOBAL_H
#define QT_COLOR_PICKER_GLOBAL_H

#include <QtCore/QtGlobal>

#if defined(QTCOLORPICKER_LIBRARY)
#    define QCP_EXPORT Q_DECL_EXPORT
#else
#    define QCP_EXPORT Q_DECL_IMPORT
#endif

#endif
