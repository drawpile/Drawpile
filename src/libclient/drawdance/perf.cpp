// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpcommon/output.h>
}
#include "libclient/drawdance/perf.h"

namespace drawdance {

bool Perf::open(const QString &path)
{
    DP_Output *output = DP_gzip_output_new_from_path(path.toUtf8().constData());
    return output ? DP_perf_open(output) : false;
}

bool Perf::close()
{
    return DP_perf_close();
}

bool Perf::isOpen()
{
    return DP_perf_is_open();
}

Perf Perf::instance{};

Perf::~Perf()
{
    if(DP_perf_is_open()) {
        if(!DP_perf_close()) {
            qWarning("Error closing perf output: %s", DP_error());
        }
    }
}

}
