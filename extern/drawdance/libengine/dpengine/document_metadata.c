/*
 * Copyright (C) 2022 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#include "document_metadata.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_DocumentMetadata {
    DP_Atomic refcount;
    bool transient;
    const int dpix;
    const int dpiy;
    const int framerate;
    const bool use_timeline;
};

struct DP_TransientDocumentMetadata {
    DP_Atomic refcount;
    bool transient;
    int dpix;
    int dpiy;
    int framerate;
    bool use_timeline;
};

#else

struct DP_DocumentMetadata {
    DP_Atomic refcount;
    bool transient;
    int dpix;
    int dpiy;
    int framerate;
    bool use_timeline;
};

#endif


static DP_TransientDocumentMetadata *
allocate_document_metadata(bool transient, int dpix, int dpiy, int framerate,
                           int use_timeline)
{
    DP_TransientDocumentMetadata *tdm = DP_malloc(sizeof(*tdm));
    *tdm = (DP_TransientDocumentMetadata){
        DP_ATOMIC_INIT(1), transient, dpix, dpiy, framerate, use_timeline};
    return tdm;
}


DP_DocumentMetadata *DP_document_metadata_new(void)
{
    return (DP_DocumentMetadata *)allocate_document_metadata(
        false, DP_DOCUMENT_METADATA_DPIX_DEFAULT,
        DP_DOCUMENT_METADATA_DPIY_DEFAULT,
        DP_DOCUMENT_METADATA_FRAMERATE_DEFAULT, false);
}

DP_DocumentMetadata *DP_document_metadata_incref(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    DP_atomic_inc(&dm->refcount);
    return dm;
}

DP_DocumentMetadata *
DP_document_metadata_incref_nullable(DP_DocumentMetadata *dm_or_null)
{
    return dm_or_null ? DP_document_metadata_incref(dm_or_null) : NULL;
}

void DP_document_metadata_decref(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    if (DP_atomic_dec(&dm->refcount)) {
        DP_free(dm);
    }
}

void DP_document_metadata_decref_nullable(DP_DocumentMetadata *dm_or_null)
{
    if (dm_or_null) {
        DP_document_metadata_decref(dm_or_null);
    }
}

int DP_document_metadata_refcount(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return DP_atomic_get(&dm->refcount);
}

bool DP_document_metadata_transient(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->transient;
}

int DP_document_metadata_dpix(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->dpix;
}

int DP_document_metadata_dpiy(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->dpiy;
}

int DP_document_metadata_framerate(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->framerate;
}

bool DP_document_metadata_use_timeline(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->use_timeline;
}


DP_TransientDocumentMetadata *
DP_transient_document_metadata_new(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return allocate_document_metadata(true, dm->dpix, dm->dpiy, dm->framerate,
                                      dm->use_timeline);
}

DP_TransientDocumentMetadata *
DP_transient_document_metadata_incref(DP_TransientDocumentMetadata *tdm)
{
    return (DP_TransientDocumentMetadata *)DP_document_metadata_incref(
        (DP_DocumentMetadata *)tdm);
}

void DP_transient_document_metadata_decref(DP_TransientDocumentMetadata *tdm)
{
    DP_document_metadata_decref((DP_DocumentMetadata *)tdm);
}

int DP_transient_document_metadata_refcount(DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_refcount((DP_DocumentMetadata *)tdm);
}

DP_DocumentMetadata *
DP_transient_document_metadata_persist(DP_TransientDocumentMetadata *tdm)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->transient = false;
    return (DP_DocumentMetadata *)tdm;
}

int DP_transient_document_metadata_dpix(DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_dpix((DP_DocumentMetadata *)tdm);
}

int DP_transient_document_metadata_dpiy(DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_dpiy((DP_DocumentMetadata *)tdm);
}

int DP_transient_document_metadata_framerate(DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_framerate((DP_DocumentMetadata *)tdm);
}

bool DP_transient_document_metadata_use_timeline(
    DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_use_timeline((DP_DocumentMetadata *)tdm);
}

void DP_transient_document_metadata_dpix_set(DP_TransientDocumentMetadata *tdm,
                                             int dpix)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->dpix = dpix;
}

void DP_transient_document_metadata_dpiy_set(DP_TransientDocumentMetadata *tdm,
                                             int dpiy)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->dpiy = dpiy;
}

void DP_transient_document_metadata_framerate_set(
    DP_TransientDocumentMetadata *tdm, int framerate)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->framerate = framerate;
}

void DP_transient_document_metadata_use_timeline_set(
    DP_TransientDocumentMetadata *tdm, int use_timeline)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->use_timeline = use_timeline != 0;
}
