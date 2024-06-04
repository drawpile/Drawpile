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
#ifndef DPENGINE_DOCUMENT_METADATA_H
#define DPENGINE_DOCUMENT_METADATA_H
#include <dpcommon/common.h>


#define DP_DOCUMENT_METADATA_DPIX_DEFAULT        72
#define DP_DOCUMENT_METADATA_DPIY_DEFAULT        72
#define DP_DOCUMENT_METADATA_FRAMERATE_DEFAULT   24
#define DP_DOCUMENT_METADATA_FRAME_COUNT_DEFAULT 24

typedef struct DP_DocumentMetadata DP_DocumentMetadata;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientDocumentMetadata DP_TransientDocumentMetadata;
#else
typedef struct DP_DocumentMetadata DP_TransientDocumentMetadata;
#endif


DP_DocumentMetadata *DP_document_metadata_new(void);

DP_DocumentMetadata *DP_document_metadata_incref(DP_DocumentMetadata *dm);

DP_DocumentMetadata *
DP_document_metadata_incref_nullable(DP_DocumentMetadata *dm_or_null);

void DP_document_metadata_decref(DP_DocumentMetadata *dm);

void DP_document_metadata_decref_nullable(DP_DocumentMetadata *dm_or_null);

int DP_document_metadata_refcount(DP_DocumentMetadata *dm);

bool DP_document_metadata_transient(DP_DocumentMetadata *dm);

int DP_document_metadata_dpix(DP_DocumentMetadata *dm);

int DP_document_metadata_dpiy(DP_DocumentMetadata *dm);

int DP_document_metadata_framerate(DP_DocumentMetadata *dm);

int DP_document_metadata_frame_count(DP_DocumentMetadata *dm);


DP_TransientDocumentMetadata *
DP_transient_document_metadata_new(DP_DocumentMetadata *dm);

DP_TransientDocumentMetadata *DP_transient_document_metadata_new_init(void);

DP_TransientDocumentMetadata *
DP_transient_document_metadata_incref(DP_TransientDocumentMetadata *tdm);

void DP_transient_document_metadata_decref(DP_TransientDocumentMetadata *tdm);

int DP_transient_document_metadata_refcount(DP_TransientDocumentMetadata *tdm);

DP_DocumentMetadata *
DP_transient_document_metadata_persist(DP_TransientDocumentMetadata *tdm);

int DP_transient_document_metadata_dpix(DP_TransientDocumentMetadata *tdm);

int DP_transient_document_metadata_dpiy(DP_TransientDocumentMetadata *tdm);

int DP_transient_document_metadata_framerate(DP_TransientDocumentMetadata *tdm);

int DP_transient_document_metadata_frame_count(
    DP_TransientDocumentMetadata *tdm);

void DP_transient_document_metadata_dpix_set(DP_TransientDocumentMetadata *tdm,
                                             int dpix);

void DP_transient_document_metadata_dpiy_set(DP_TransientDocumentMetadata *tdm,
                                             int dpiy);

void DP_transient_document_metadata_framerate_set(
    DP_TransientDocumentMetadata *tdm, int framerate);

void DP_transient_document_metadata_frame_count_set(
    DP_TransientDocumentMetadata *tdm, int frame_count);

void DP_transient_document_metadata_use_timeline_set(
    DP_TransientDocumentMetadata *tdm, int use_timeline);


#endif
