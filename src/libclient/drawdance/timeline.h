#ifndef DRAWDANCE_TIMELINE_H
#define DRAWDANCE_TIMELINE_H

#include "libclient/drawdance/frame.h"

struct DP_Timeline;

namespace drawdance {

class Timeline final {
public:
    static Timeline inc(DP_Timeline *tl);
    static Timeline noinc(DP_Timeline *tl);

    Timeline();
    Timeline(const Timeline &other);
    Timeline(Timeline &&other);

    Timeline &operator=(const Timeline &other);
    Timeline &operator=(Timeline &&other);

    ~Timeline();

    bool isNull() const;

    int frameCount() const;

    drawdance::Frame frameAt(int i) const;

private:
    explicit Timeline(DP_Timeline *tl);

    DP_Timeline *m_data;
};

}

#endif
