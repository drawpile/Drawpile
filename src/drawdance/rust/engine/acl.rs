use crate::{DP_AclState, DP_acl_state_free, DP_acl_state_new};

pub struct AclState {
    acls: *mut DP_AclState,
}

impl AclState {
    pub fn as_ptr(&mut self) -> *mut DP_AclState {
        self.acls
    }
}

impl Default for AclState {
    fn default() -> Self {
        let acls = unsafe { DP_acl_state_new() };
        AclState { acls }
    }
}

impl Drop for AclState {
    fn drop(&mut self) {
        unsafe { DP_acl_state_free(self.acls) }
    }
}
