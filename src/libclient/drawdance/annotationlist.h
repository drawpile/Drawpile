// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_ANNOTATION_LIST_H
#define DRAWDANCE_ANNOTATION_LIST_H

#include "libclient/drawdance/annotation.h"

struct DP_AnnotationList;

namespace drawdance {

class AnnotationList final {
public:
    static AnnotationList inc(DP_AnnotationList *al);
    static AnnotationList noinc(DP_AnnotationList *al);

    AnnotationList(const AnnotationList &other);
    AnnotationList(AnnotationList &&other);

    AnnotationList &operator=(const AnnotationList &other);
    AnnotationList &operator=(AnnotationList &&other);

    ~AnnotationList();

    bool isNull() const;

    int count() const;

    Annotation at(int index) const;

private:
    explicit AnnotationList(DP_AnnotationList *al);

    DP_AnnotationList *m_data;
};

}

#endif
