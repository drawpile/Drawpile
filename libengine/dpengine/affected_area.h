#ifndef DP_AFFECTED_AREA
#define DP_AFFECTED_AREA
#include <dpcommon/common.h>
#include <dpcommon/geom.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Message DP_Message;


typedef enum DP_AffectedDomain {
    DP_AFFECTED_DOMAIN_USER_ATTRS,
    DP_AFFECTED_DOMAIN_LAYER_ATTRS,
    DP_AFFECTED_DOMAIN_ANNOTATIONS,
    DP_AFFECTED_DOMAIN_PIXELS,
    DP_AFFECTED_DOMAIN_CANVAS_BACKGROUND,
    DP_AFFECTED_DOMAIN_EVERYTHING,
} DP_AffectedDomain;

typedef struct DP_AffectedArea {
    DP_AffectedDomain domain;
    int affected_id; // layer or annotation id
    DP_Rect bounds;
} DP_AffectedArea;


DP_AffectedArea DP_affected_area_make(DP_Message *msg, DP_CanvasState *cs);

bool DP_affected_area_concurrent_with(const DP_AffectedArea *aa,
                                      const DP_AffectedArea *other);


#endif
