extern "C" {
#include <dpengine/annotation_list.h>
}

#include "libclient/drawdance/annotationlist.h"

namespace drawdance {

AnnotationList AnnotationList::inc(DP_AnnotationList *al)
{
    return AnnotationList{DP_annotation_list_incref_nullable(al)};
}

AnnotationList AnnotationList::noinc(DP_AnnotationList *al)
{
    return AnnotationList{al};
}

AnnotationList::AnnotationList(const AnnotationList &other)
    : AnnotationList{DP_annotation_list_incref_nullable(other.m_data)}
{
}

AnnotationList::AnnotationList(AnnotationList &&other)
    : AnnotationList{other.m_data}
{
    other.m_data = nullptr;
}

AnnotationList &AnnotationList::operator=(const AnnotationList &other)
{
    DP_annotation_list_decref_nullable(m_data);
    m_data = DP_annotation_list_incref_nullable(other.m_data);
    return *this;
}

AnnotationList &AnnotationList::operator=(AnnotationList &&other)
{
    DP_annotation_list_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

AnnotationList::~AnnotationList()
{
    DP_annotation_list_decref_nullable(m_data);
}

bool AnnotationList::isNull() const
{
    return !m_data;
}

int AnnotationList::count() const
{
    return DP_annotation_list_count(m_data);
}

Annotation AnnotationList::at(int index) const
{
    return Annotation::inc(DP_annotation_list_at_noinc(m_data, index));
}

AnnotationList::AnnotationList(DP_AnnotationList *al)
    : m_data{al}
{
}

}
