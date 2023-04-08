// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_FRAME_H
#define DRAWDANCE_FRAME_H

struct DP_Frame;

namespace drawdance {

class Frame final {
public:
    static Frame inc(DP_Frame *f);
    static Frame noinc(DP_Frame *f);

    Frame(const Frame &other);
    Frame(Frame &&other);

    Frame &operator=(const Frame &other);
    Frame &operator=(Frame &&other);

    ~Frame();

    bool isNull() const;

    int layerIdCount() const;

    int layerIdAt(int index) const;

private:
    explicit Frame(DP_Frame *f);

    DP_Frame *m_data;
};

}

#endif
