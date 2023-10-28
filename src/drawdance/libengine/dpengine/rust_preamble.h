#include "load.h"
#include "save.h"
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerProps DP_TransientLayerProps;
typedef struct DP_TransientTrack DP_TransientTrack;
#else
typedef struct DP_LayerProps DP_TransientLayerProps;
typedef struct DP_Track DP_TransientTrack;
#endif
