#include "save.h"
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerProps DP_TransientLayerProps;
#else
typedef struct DP_LayerProps DP_TransientLayerProps;
#endif
