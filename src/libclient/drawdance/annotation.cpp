extern "C" {
#include <dpengine/annotation.h>
}

#include "libclient/drawdance/annotation.h"

namespace drawdance {

Annotation Annotation::null()
{
    return Annotation{nullptr};
}

Annotation Annotation::inc(DP_Annotation *a)
{
    return Annotation{DP_annotation_incref_nullable(a)};
}

Annotation Annotation::noinc(DP_Annotation *a)
{
    return Annotation{a};
}

Annotation::Annotation(const Annotation &other)
    : Annotation{DP_annotation_incref_nullable(other.m_data)}
{
}

Annotation::Annotation(Annotation &&other)
    : Annotation{other.m_data}
{
    other.m_data = nullptr;
}

Annotation &Annotation::operator=(const Annotation &other)
{
    DP_annotation_decref_nullable(m_data);
    m_data = DP_annotation_incref_nullable(other.m_data);
    return *this;
}

Annotation &Annotation::operator=(Annotation &&other)
{
    DP_annotation_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

Annotation::~Annotation()
{
    DP_annotation_decref_nullable(m_data);
}

bool Annotation::isNull() const
{
    return !m_data;
}

int Annotation::id() const
{
    return DP_annotation_id(m_data);
}

int Annotation::x() const
{
    return DP_annotation_x(m_data);
}

int Annotation::y() const
{
    return DP_annotation_y(m_data);
}

int Annotation::width() const
{
    return DP_annotation_width(m_data);
}

int Annotation::height() const
{
    return DP_annotation_height(m_data);
}

QRect Annotation::bounds() const
{
    return QRect{x(), y(), width(), height()};
}

bool Annotation::protect() const
{
    return DP_annotation_protect(m_data);
}

int Annotation::valign() const
{
    return DP_annotation_valign(m_data);
}

QColor Annotation::backgroundColor() const
{
    return QColor::fromRgba(DP_annotation_background_color(m_data));
}

QByteArray Annotation::textBytes() const
{
    size_t length;
    const char *text = DP_annotation_text(m_data, &length);
    return QByteArray::fromRawData(text, length);
}

QString Annotation::text() const
{
    return QString::fromUtf8(textBytes());
}

Annotation::Annotation(DP_Annotation *a)
    : m_data{a}
{
}

}
