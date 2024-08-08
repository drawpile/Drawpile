#ifndef LIBCLIENT_IMPORT_LOADMESSAGE_H
#define LIBCLIENT_IMPORT_LOADMESSAGE_H
extern "C" {
#include <dpengine/load_enums.h>
}
#include <QString>

namespace impex {

QString getLoadResultMessage(DP_LoadResult result);

bool shouldIncludeLoadResultDpError(DP_LoadResult result);

}

#endif
