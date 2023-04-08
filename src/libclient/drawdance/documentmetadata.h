// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_DOCUMENT_METADATA_H
#define DRAWDANCE_DOCUMENT_METADATA_H

struct DP_DocumentMetadata;

namespace drawdance {

class DocumentMetadata final {
public:
    static DocumentMetadata inc(DP_DocumentMetadata *dm);
    static DocumentMetadata noinc(DP_DocumentMetadata *dm);

    DocumentMetadata(const DocumentMetadata &other);
    DocumentMetadata(DocumentMetadata &&other);

    DocumentMetadata &operator=(const DocumentMetadata &other);
    DocumentMetadata &operator=(DocumentMetadata &&other);

    ~DocumentMetadata();

    bool isNull() const;

    int dpix() const;
    int dpiy() const;
    int framerate() const;
    bool useTimeline() const;

private:
    explicit DocumentMetadata(DP_DocumentMetadata *dm);

    DP_DocumentMetadata *m_data;
};

}

#endif

