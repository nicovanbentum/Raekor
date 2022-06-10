use std::ops::DerefMut;

use windows::Win32::Graphics::Direct3D12::{D3D12_RESOURCE_BARRIER, D3D12_RESOURCE_STATES, D3D12_RESOURCE_BARRIER_FLAGS, ID3D12Resource, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_NONE};

pub struct CD3DX12_RESOURCE_BARRIER {
    barrier : D3D12_RESOURCE_BARRIER
}

impl Default for CD3DX12_RESOURCE_BARRIER {
    fn default() -> Self {
        Self { barrier: D3D12_RESOURCE_BARRIER::default() }
    }
}

impl CD3DX12_RESOURCE_BARRIER {
    pub fn Transition(resource : ID3D12Resource, stateBefore : D3D12_RESOURCE_STATES, stateAfter : D3D12_RESOURCE_STATES, subresource : Option<u32>, flags : Option<D3D12_RESOURCE_BARRIER_FLAGS>) -> Self {
        let mut barrier = D3D12_RESOURCE_BARRIER::default();

        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = flags.unwrap_or(D3D12_RESOURCE_BARRIER_FLAG_NONE);

        unsafe {
            barrier.Anonymous.Transition.deref_mut().pResource = Some(resource);
            barrier.Anonymous.Transition.deref_mut().StateAfter = stateAfter;
            barrier.Anonymous.Transition.deref_mut().StateBefore = stateBefore;
            barrier.Anonymous.Transition.deref_mut().Subresource = subresource.unwrap_or(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        }

        Self {
            barrier
        }
    }
}