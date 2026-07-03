// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/strings.h"
#include <QCoreApplication>

namespace strings {

QString percent()
{
    //: Percent, put after numbers as a unit, like 42%. Unless your language
    //: uses a different symbol or something, leave this as it is.
    return QCoreApplication::translate("Units", "%");
}

QString px()
{
    //: Abbreviation for pixels, put after a number as a unit, like 42px. Unless
    //: your language calls pixels something different, leave this as it is.
    return QCoreApplication::translate("Units", "px");
}

}
