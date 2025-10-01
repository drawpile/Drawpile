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
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_DocumentMetadata {
    DP_Atomic refcount;
    bool transient;
    const int dpix;
    const int dpiy;
    const int framerate;
    const int framerate_fraction;
    const int frame_count;
    const int frame_range_first;
    const int frame_range_last;
};

struct DP_TransientDocumentMetadata {
    DP_Atomic refcount;
    bool transient;
    int dpix;
    int dpiy;
    int framerate;
    int framerate_fraction;
    int frame_count;
    int frame_range_first;
    int frame_range_last;
};

#else

struct DP_DocumentMetadata {
    DP_Atomic refcount;
    bool transient;
    int dpix;
    int dpiy;
    int framerate;
    int framerate_fraction;
    int frame_count;
    int frame_range_first;
    int frame_range_last;
};

#endif


void DP_document_metadata_effective_framerate_split(double effective_framerate,
                                                    int *out_whole,
                                                    int *out_fraction)
{
    DP_ASSERT(out_whole);
    DP_ASSERT(out_fraction);

    int whole = DP_double_to_int(effective_framerate);
    int fraction;
    if (whole < 0) {
        fraction = 1;
    }
    else {
        double multiplier = DP_DOCUMENT_METADATA_FRAMERATE_FRACTION_MULTIPLIER;
        fraction = DP_max_int(
            0, DP_double_to_int(((effective_framerate - DP_int_to_double(whole))
                                 * multiplier)
                                + 0.5));
        if (fraction == 0 && whole == 0) {
            fraction = 1;
        }
    }

    *out_whole = whole;
    *out_fraction = fraction;
}


static void *allocate_document_metadata(bool transient, int dpix, int dpiy,
                                        int framerate, int framerate_fraction,
                                        int frame_count, int frame_range_first,
                                        int frame_range_last)
{
    DP_TransientDocumentMetadata *tdm = DP_malloc(sizeof(*tdm));
    *tdm = (DP_TransientDocumentMetadata){
        DP_ATOMIC_INIT(1), transient,          dpix,        dpiy,
        framerate,         framerate_fraction, frame_count, frame_range_first,
        frame_range_last};
    return tdm;
}

static void *allocate_document_metadata_default(bool transient)
{
    return allocate_document_metadata(
        transient, DP_DOCUMENT_METADATA_DPIX_DEFAULT,
        DP_DOCUMENT_METADATA_DPIY_DEFAULT,
        DP_DOCUMENT_METADATA_FRAMERATE_DEFAULT,
        DP_DOCUMENT_METADATA_FRAMERATE_FRACTION_DEFAULT,
        DP_DOCUMENT_METADATA_FRAME_COUNT_DEFAULT,
        DP_DOCUMENT_METADATA_FRAME_RANGE_FIRST_DEFAULT,
        DP_DOCUMENT_METADATA_FRAME_RANGE_LAST_DEFAULT);
}


DP_DocumentMetadata *DP_document_metadata_new(void)
{
    return allocate_document_metadata_default(false);
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

int DP_document_metadata_framerate_fraction(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->framerate_fraction;
}

double DP_document_metadata_effective_framerate(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    double multiplier = DP_DOCUMENT_METADATA_FRAMERATE_FRACTION_MULTIPLIER;
    double fps = DP_int_to_double(dm->framerate)
               + (DP_int_to_double(dm->framerate_fraction) / multiplier);
    return DP_max_double(1.0 / multiplier, fps);
}

int DP_document_metadata_frame_count(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->frame_count;
}

int DP_document_metadata_frame_range_first(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return dm->frame_range_first;
}

int DP_document_metadata_frame_range_last(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return DP_min_int(INT32_MAX - 1, dm->frame_range_last);
}

bool DP_document_metadata_effective_frame_range(DP_DocumentMetadata *dm,
                                                int *out_first, int *out_last)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);

    int limit = DP_max_int(1, dm->frame_count) - 1;
    int first = DP_min_int(limit, dm->frame_range_first);
    int last = DP_min_int(limit, dm->frame_range_last);
    bool range_valid = first >= 0 && last >= 0 && first <= last;

    if (out_first) {
        *out_first = range_valid ? first : 0;
    }
    if (out_last) {
        *out_last = range_valid ? last : limit;
    }
    return range_valid;
}


DP_TransientDocumentMetadata *
DP_transient_document_metadata_new(DP_DocumentMetadata *dm)
{
    DP_ASSERT(dm);
    DP_ASSERT(DP_atomic_get(&dm->refcount) > 0);
    return allocate_document_metadata(
        true, dm->dpix, dm->dpiy, dm->framerate, dm->framerate_fraction,
        dm->frame_count, dm->frame_range_first, dm->frame_range_last);
}

DP_TransientDocumentMetadata *DP_transient_document_metadata_new_init(void)
{
    return allocate_document_metadata_default(true);
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

int DP_transient_document_metadata_framerate_fraction(
    DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_framerate_fraction((DP_DocumentMetadata *)tdm);
}

double DP_transient_document_metadata_effective_framerate(
    DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_effective_framerate((DP_DocumentMetadata *)tdm);
}

int DP_transient_document_metadata_frame_count(
    DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_frame_count((DP_DocumentMetadata *)tdm);
}

int DP_transient_document_metadata_frame_range_first(
    DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_frame_range_first((DP_DocumentMetadata *)tdm);
}

int DP_transient_document_metadata_frame_range_last(
    DP_TransientDocumentMetadata *tdm)
{
    return DP_document_metadata_frame_range_last((DP_DocumentMetadata *)tdm);
}

bool DP_transient_document_metadata_effective_frame_range(
    DP_TransientDocumentMetadata *tdm, int *out_first, int *out_last)
{
    return DP_document_metadata_effective_frame_range(
        (DP_DocumentMetadata *)tdm, out_first, out_last);
}

void DP_transient_document_metadata_dpix_set(DP_TransientDocumentMetadata *tdm,
                                             int dpix)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->dpix = DP_max_int(1, dpix);
}

void DP_transient_document_metadata_dpiy_set(DP_TransientDocumentMetadata *tdm,
                                             int dpiy)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->dpiy = DP_max_int(1, dpiy);
}

void DP_transient_document_metadata_framerate_set(
    DP_TransientDocumentMetadata *tdm, int framerate)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->framerate = DP_max_int(0, framerate);
}

void DP_transient_document_metadata_framerate_fraction_set(
    DP_TransientDocumentMetadata *tdm, int framerate_fraction)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->framerate_fraction =
        DP_clamp_int(0, framerate_fraction,
                     DP_DOCUMENT_METADATA_FRAMERATE_FRACTION_MULTIPLIER - 1);
}

void DP_transient_document_metadata_effective_framerate_set(
    DP_TransientDocumentMetadata *tdm, double effective_framerate)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    int whole, fraction;
    DP_document_metadata_effective_framerate_split(effective_framerate, &whole,
                                                   &fraction);
    DP_transient_document_metadata_framerate_set(tdm, whole);
    DP_transient_document_metadata_framerate_fraction_set(tdm, fraction);
}

void DP_transient_document_metadata_frame_count_set(
    DP_TransientDocumentMetadata *tdm, int frame_count)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->frame_count = DP_clamp_int(frame_count, 1, INT32_MAX);
}

void DP_transient_document_metadata_frame_range_first_set(
    DP_TransientDocumentMetadata *tdm, int frame_range_first)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->frame_range_first = frame_range_first;
}

void DP_transient_document_metadata_frame_range_last_set(
    DP_TransientDocumentMetadata *tdm, int frame_range_last)
{
    DP_ASSERT(tdm);
    DP_ASSERT(DP_atomic_get(&tdm->refcount) > 0);
    DP_ASSERT(tdm->transient);
    tdm->frame_range_last = DP_min_int(INT32_MAX - 1, frame_range_last);
}
