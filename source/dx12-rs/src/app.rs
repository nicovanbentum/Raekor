use windows::Win32::{Graphics::Direct3D12::ID3D12Fence, Foundation::HANDLE};

pub struct BackBufferData {
    mFenceValue : u64,
    mBackbufferRTV : u32,
}

pub struct App {
    mFrameIndex : u32,
    mFence : ID3D12Fence,
    mFenceEvent : HANDLE,
    mBackbufferData : [BackBufferData; 2]
}

impl App {
    fn on_update(dt : f32) {

    }
}

