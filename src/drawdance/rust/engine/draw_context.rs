use crate::{DP_DrawContext, DP_draw_context_free, DP_draw_context_new};

pub struct DrawContext {
    dc: *mut DP_DrawContext,
}

impl DrawContext {
    pub fn as_ptr(&mut self) -> *mut DP_DrawContext {
        self.dc
    }
}

impl Default for DrawContext {
    fn default() -> Self {
        let dc = unsafe { DP_draw_context_new() };
        DrawContext { dc }
    }
}

impl Drop for DrawContext {
    fn drop(&mut self) {
        unsafe { DP_draw_context_free(self.dc) }
    }
}
