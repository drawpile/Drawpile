// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/document_metadata.h>
}

#include "libclient/drawdance/documentmetadata.h"

namespace drawdance {

DocumentMetadata DocumentMetadata::inc(DP_DocumentMetadata *dm)
{
    return DocumentMetadata{DP_document_metadata_incref_nullable(dm)};
}

DocumentMetadata DocumentMetadata::noinc(DP_DocumentMetadata *dm)
{
    return DocumentMetadata{dm};
}

DocumentMetadata::DocumentMetadata(const DocumentMetadata &other)
    : DocumentMetadata{DP_document_metadata_incref_nullable(other.m_data)}
{
}

DocumentMetadata::DocumentMetadata(DocumentMetadata &&other)
    : DocumentMetadata{other.m_data}
{
    other.m_data = nullptr;
}

DocumentMetadata &DocumentMetadata::operator=(const DocumentMetadata &other)
{
    DP_document_metadata_decref_nullable(m_data);
    m_data = DP_document_metadata_incref_nullable(other.m_data);
    return *this;
}

DocumentMetadata &DocumentMetadata::operator=(DocumentMetadata &&other)
{
    DP_document_metadata_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

DocumentMetadata::~DocumentMetadata()
{
    DP_document_metadata_decref_nullable(m_data);
}

bool DocumentMetadata::isNull() const
{
    return !m_data;
}

int DocumentMetadata::dpix() const
{
    return DP_document_metadata_dpix(m_data);
}

int DocumentMetadata::dpiy() const
{
    return DP_document_metadata_dpix(m_data);
}

int DocumentMetadata::framerate() const
{
    return DP_document_metadata_framerate(m_data);
}

int DocumentMetadata::frameCount() const
{
    return DP_document_metadata_frame_count(m_data);
}

DocumentMetadata::DocumentMetadata(DP_DocumentMetadata *dm)
    : m_data{dm}
{
}

}
