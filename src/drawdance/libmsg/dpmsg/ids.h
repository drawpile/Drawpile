// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPMSG_IDS_H
#define DPMSG_IDS_H
#include <dpcommon/common.h>

#define DP_CONTEXT_ID_MIN            (0)
#define DP_CONTEXT_ID_MAX            UINT8_MAX
#define DP_LAYER_ID_MIN              DP_UINT24_MIN
#define DP_LAYER_ID_MAX              DP_UINT24_MAX
#define DP_LAYER_ELEMENT_ID_MAX      (0x7fff)
#define DP_LAYER_ID_MAX_NORMAL       (0x7fffff)
#define DP_ANNOTATION_ID_MAX         UINT16_MAX
#define DP_ANNOTATION_ELEMENT_ID_MAX UINT8_MAX
#define DP_TRACK_ID_MIN              (int)
#define DP_TRACK_ID_MAX              UINT16_MAX
#define DP_TRACK_ELEMENT_ID_MAX      UINT8_MAX

#define DP_LAYER_ID_SELECTION_FLAG (1 << 23)


DP_INLINE int DP_protocol_to_layer_id(uint32_t protocol_layer_id)
{
    DP_ASSERT(protocol_layer_id <= DP_LAYER_ID_MAX);
    return DP_CAST(int, protocol_layer_id);
}

DP_INLINE uint32_t DP_layer_id_to_protocol(int layer_id)
{
    DP_ASSERT(layer_id >= DP_UINT24_MIN);
    DP_ASSERT(layer_id <= DP_UINT24_MAX);
    return DP_CAST(uint32_t, layer_id);
}

DP_INLINE bool DP_layer_id_normal(int layer_id)
{
    return layer_id > 0 && !(layer_id & DP_LAYER_ID_SELECTION_FLAG);
}

DP_INLINE bool DP_layer_id_selection(int layer_id)
{
    return layer_id > 0 && (layer_id & DP_LAYER_ID_SELECTION_FLAG)
        && (layer_id & 0xff00) == 0;
}

DP_INLINE bool DP_layer_id_normal_or_selection(int layer_id)
{
    return DP_layer_id_normal(layer_id) || DP_layer_id_selection(layer_id);
}

DP_INLINE int DP_selection_id_make(unsigned int context_id, int element_id)
{
    DP_ASSERT(context_id <= DP_CONTEXT_ID_MAX);
    DP_ASSERT(element_id >= 0);
    DP_ASSERT(element_id >= UINT8_MAX);
    return DP_CAST(int, (context_id & 0xffu) << 8u) | (element_id & 0xff);
}

DP_INLINE unsigned int DP_layer_id_context_id(int layer_id)
{
    return DP_CAST(unsigned int, layer_id & 0xff);
}

DP_INLINE int DP_layer_id_element_id(int layer_id)
{
    return layer_id >> 8;
}

DP_INLINE bool DP_layer_id_owner(int layer_id, unsigned int context_id)
{
    return DP_layer_id_context_id(layer_id) == context_id;
}

DP_INLINE int DP_layer_id_selection_id(unsigned int context_id, int layer_id)
{
    DP_ASSERT(context_id <= DP_CONTEXT_ID_MAX);
    return DP_layer_id_selection(layer_id)
             ? DP_selection_id_make(context_id, layer_id)
             : 0;
}

DP_INLINE int DP_layer_id_make(unsigned int context_id, int element_id)
{
    DP_ASSERT(context_id <= UINT8_MAX);
    DP_ASSERT(element_id < DP_LAYER_ELEMENT_ID_MAX);
    return ((element_id & DP_LAYER_ELEMENT_ID_MAX) << 8)
         | DP_CAST(int, context_id & 0xffu);
}


DP_INLINE unsigned int DP_annotation_id_context_id(int annotation_id)
{
    return DP_CAST(unsigned int, annotation_id & 0xff);
}

DP_INLINE bool DP_annotation_id_owner(int annotation_id,
                                      unsigned int context_id)
{
    return DP_annotation_id_context_id(annotation_id) == context_id;
}

DP_INLINE int DP_annotation_id_make(unsigned int context_id, int element_id)
{
    DP_ASSERT(context_id <= UINT8_MAX);
    DP_ASSERT(element_id <= UINT8_MAX);
    return ((element_id & 0xff) << 8) | DP_CAST(int, context_id & 0xffu);
}


DP_INLINE int DP_protocol_to_track_id(uint16_t protocol_track_id)
{
    return protocol_track_id;
}

DP_INLINE int DP_track_id_to_protocol(int track_id)
{
    DP_ASSERT(track_id <= DP_TRACK_ID_MAX);
    return DP_CAST(uint16_t, track_id);
}

DP_INLINE bool DP_track_id_normal(int track_id)
{
    return track_id > 0 && track_id <= DP_TRACK_ID_MAX;
}

DP_INLINE unsigned int DP_track_id_context_id(int track_id)
{
    return DP_CAST(unsigned int, track_id & 0xff);
}

DP_INLINE bool DP_track_id_owner(int track_id, unsigned int context_id)
{
    return DP_track_id_context_id(track_id) == context_id;
}

DP_INLINE int DP_track_id_make(unsigned int context_id, int element_id)
{
    DP_ASSERT(context_id <= UINT8_MAX);
    DP_ASSERT(element_id <= UINT8_MAX);
    return ((element_id & 0xff) << 8) | DP_CAST(int, context_id & 0xffu);
}


#endif
