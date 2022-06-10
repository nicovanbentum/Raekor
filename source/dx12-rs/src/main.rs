#[no_mangle]
pub static D3D12SDKVersion: u32 = 600;

#[no_mangle]
pub static D3D12SDKPath: &[u8; 9] = b".\\D3D12\\\0";

mod barrier;
use barrier::*;

mod components;
use components::*;

use std::{error::Error, fmt::Display, collections::HashMap};

use itertools::Itertools;
use itertools::izip;
use rayon::prelude::*;
use bevy_ecs::prelude::*;
use hassle_rs::{compile_hlsl, validate_dxil};
use native_dialog::{FileDialog, MessageDialog, MessageType};
use windows::{ core::*, Win32::Foundation::*, Win32::{System::Threading::*, Graphics::Dxgi::Common::*, Graphics::Dxgi::*, Graphics::Direct3D12::*, Graphics::Direct3D::*}};
use winit::{event_loop::*, event::{WindowEvent, Event}, window::WindowBuilder, platform::{windows::WindowExtWindows, run_return::EventLoopExtRunReturn}};

fn get_adapter(factory: &IDXGIFactory4) -> Result<IDXGIAdapter1> {
    for index in 0.. {
        let adapter = unsafe { factory.EnumAdapters1(index) } ?;
        let desc = unsafe { adapter.GetDesc1() } ?;

        if desc.Flags != 0 {
            continue;
        }

        return Ok(adapter);
    }

    unreachable!();
}

struct DescriptorHeap {
    mHeapIncrement : u32,
    mFreeIndices : Vec<u32>,
    mHeap : Option<ID3D12DescriptorHeap>,
    mHeapPtr : D3D12_CPU_DESCRIPTOR_HANDLE
}

impl DescriptorHeap {
    fn new(device : &ID3D12Device, heap_type : D3D12_DESCRIPTOR_HEAP_TYPE, count : u32, flags : D3D12_DESCRIPTOR_HEAP_FLAGS) -> Self {
        let desc = D3D12_DESCRIPTOR_HEAP_DESC {
            Type : heap_type,
            Flags : flags,
            NumDescriptors : count,
            NodeMask : 0
        };

        let result_heap : Result<ID3D12DescriptorHeap> = unsafe { device.CreateDescriptorHeap(&desc) };

        match result_heap {
            Ok(heap) => { 
                let start = unsafe { heap.GetCPUDescriptorHandleForHeapStart() };
                return Self {
                    mHeapIncrement : unsafe { device.GetDescriptorHandleIncrementSize(heap_type) },
                    mFreeIndices : vec![],
                    mHeap : Some(heap),
                    mHeapPtr : start
            }
        },
            Err(error) => panic!("{}", error.message())
        };
    }
}

pub struct BackBufferData {
    fence_value : u64,
    cmd_list : ID3D12GraphicsCommandList4,
    cmd_allocator : ID3D12CommandAllocator
}

impl BackBufferData {
    fn new(device: &ID3D12Device5) -> Self {
        let command_allocator = unsafe { device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT) };

        let command_list = match command_allocator {
            Ok(ref a) => unsafe { device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, a, None) },
            Err(_e) => panic!()
        };

        Self { 
            fence_value : 0,
            cmd_list : command_list.unwrap(),
            cmd_allocator : command_allocator.unwrap()
        }
    }
}

fn main() -> Result<()> {
    let mut event_loop = EventLoop::new();
    let window = WindowBuilder::new().build(&event_loop).unwrap();

    let mut debug : Option<ID3D12Debug1> = None;
    unsafe { match D3D12GetDebugInterface(&mut debug) {
        Ok(()) => match debug {
            None => panic!(),
            Some(d) => d.EnableDebugLayer()
        }
        Err(_e) => panic!()
    } };

    let factory : IDXGIFactory4 = unsafe { CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG) }?;
    let adapter = get_adapter(&factory)?;

    let mut device_opt : Option<ID3D12Device5> = None;
    unsafe { D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, &mut device_opt).expect("Device creation error") };
    let device = device_opt.unwrap();

    let frame_count = 2;

    let desc = DXGI_SWAP_CHAIN_DESC1 {
        BufferCount : frame_count,
        Width : window.inner_size().width,
        Height : window.inner_size().height,
        Format : DXGI_FORMAT_B8G8R8A8_UNORM,
        BufferUsage : DXGI_USAGE_RENDER_TARGET_OUTPUT,
        SwapEffect : DXGI_SWAP_EFFECT_FLIP_DISCARD,
        SampleDesc : DXGI_SAMPLE_DESC {
            Count : 1,
            Quality : 0
        },
        Stereo: BOOL::from(false),
        Scaling: DXGI_SCALING_STRETCH,
        AlphaMode: DXGI_ALPHA_MODE_UNSPECIFIED,
        Flags: 0,
    };

    
    let hwnd = HWND(window.hwnd() as isize);
    
    let queue_desc : D3D12_COMMAND_QUEUE_DESC = Default::default();
    let queue : ID3D12CommandQueue = unsafe { device.CreateCommandQueue(&queue_desc)? };
    
    let swapchain : IDXGISwapChain3 = unsafe {
        let res = factory.CreateSwapChainForHwnd(&queue, &hwnd, &desc, std::ptr::null(), None)?;
        res.cast()?
    };
    
    let mut frame_index = unsafe { swapchain.GetCurrentBackBufferIndex() };
    
    let mut backbuffers = [
        BackBufferData::new(&device),
        BackBufferData::new(&device)
    ];

    for backbuffer in &backbuffers {
        unsafe { backbuffer.cmd_list.Close()? };
    }

    let fence : ID3D12Fence = unsafe { device.CreateFence(0, D3D12_FENCE_FLAG_NONE)? };
    let hevent : HANDLE = unsafe { CreateEventA(std::ptr::null(), false, false, PCSTR(std::ptr::null()))? };

    let rtv_heap = DescriptorHeap::new(&(device.cast()?), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, u16::max_value().into(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    let dsv_heap = DescriptorHeap::new(&(device.cast()?), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, u16::max_value().into(), D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    let sampler_heap = DescriptorHeap::new(&(device.cast()?), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2043, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    let cbv_srv_uav_heap = DescriptorHeap::new(&(device.cast()?), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, u16::max_value().into(), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    let shader_directory = "../../assets/system/shaders/DirectX";

    if let Ok(entries) = std::fs::read_dir(shader_directory) {
        let collected_entries : Vec<std::result::Result<std::fs::DirEntry, std::io::Error>> = entries.collect();
        let shaders : Vec<Option<(String, Vec<u8>)>> = collected_entries.into_par_iter().map(|entry| {
            if let Ok(file) = entry {
                if let Ok(file_type) = file.file_type() {
                    let file_name = file.file_name().clone();
                    let path = std::path::Path::new(file_name.to_str().unwrap_or_default());
                    let extension = path.extension().unwrap_or_default();

                    if file_type.is_dir() || extension != "hlsl" { 
                        return None;
                    }

                    let stem = path.file_stem().unwrap_or_default().to_str().unwrap_or_default();

                    let len = stem.chars().count();
                    let type_str : String = String::from(stem).drain(len-2..len).collect();
                    assert!(type_str.len() == 2);

                    if let Ok(contents) = std::fs::read_to_string(file.path()) {
                        let dxil = compile_hlsl(
                            stem, 
                            contents.as_str(), 
                            "main", 
                            format!("{}_6_6", type_str.to_lowercase()).as_str(), 
                            &vec!["-Zi", "-Qembed_debug", "-Od", "-I", shader_directory],
                            &vec![]
                        );

                        match dxil {
                            Ok(dxil) => {
                                let result = validate_dxil(&dxil);

                                return match result {
                                    Ok(res) => {
                                        println!("Succesfully compiled: {}", stem);
                                        Some((String::from(stem), res))
                                    }

                                    Err(error) => {
                                        println!("validation failed: {}", &error);
                                        None
                                    }
                                };



                            },

                            Err(error) => {
                                println!("Failed to compile shader: {}", error);
                            }
                        }

                    }
                }
            }
            
            return None;

        }).collect();
    }

    let mut world = World::default();

    if let Ok(Some(path)) = FileDialog::new().add_filter("GLTF File", &["gltf", "glb"]).show_open_single_file() {
        println!("Opened file: {}", path.display());

        if let Ok((document, buffers, images)) = gltf::import(path) {

            let mut material_map : Vec<u32>;

            for material in document.materials() {
               let entity = world.spawn().insert(Material::new(&material)).id();
            }

            let find_material_index = | query : Query<&Material> | { 
                for material in query.iter() {

                }
            };

            let mut query = world.query::<(Entity, &Material)>();
            
            for mesh in document.meshes() {
                let mut m = Mesh::default();

                for prim in mesh.primitives() {
                    let material = prim.material();

                    let reader = prim.reader(|buffer| Some(&buffers[buffer.index()]));
                    if let Some(positions) = reader.read_positions() {
                        for pos in positions {
                            m.positions.push(glam::Vec3::from(pos));
                        }
                    }

                    if let Some(indices) = reader.read_indices() {
                        for index in indices.into_u32() {
                            m.indices.push(index);
                        }
                    }

                    for (uvs, normals, tangents) in izip!(reader.read_tex_coords(0), reader.read_normals(), reader.read_tangents()) {
                        for (uv, normal, tangent) in izip!(uvs.into_f32(), normals, tangents) {
                            m.attributes.extend_from_slice(&uv);
                            m.attributes.extend_from_slice(&normal);
                            m.attributes.extend_from_slice(&tangent);
                            
                            for (entity, material) in query.iter(&mut world) {
                                if *material == Material::new(&prim.material()) {
                                    m.attributes.push(entity.id() as f32);
                                }
                            }
                        }
                    }
                }
            }
        }

    }

    let mut quit = false;

    while !quit {
        event_loop.run_return(|event, _, control_flow| {
            *control_flow = ControlFlow::Wait;

            match event {
                Event::WindowEvent {
                    event: WindowEvent::CloseRequested,
                    ..
                } => {
                    quit = true;
                }
                Event::MainEventsCleared => {
                    *control_flow = ControlFlow::Exit;
                }
                _ => (),
            }
        });

        // Render stuff
        let backbuffer = &mut backbuffers[frame_index as usize];
        let completed_value = unsafe { fence.GetCompletedValue() };

        if completed_value < backbuffer.fence_value {
            unsafe { 
                fence.SetEventOnCompletion(backbuffer.fence_value, &hevent)?;
                WaitForSingleObjectEx(&hevent, u32::max_value(), false);
            };
        }

        unsafe {
            backbuffer.cmd_allocator.Reset()?;
            backbuffer.cmd_list.Reset(&backbuffer.cmd_allocator, None)?;
        }

        unsafe { backbuffer.cmd_list.Close()? };
        
        let submit_list : ID3D12CommandList = backbuffer.cmd_list.cast().unwrap();
        let cmd_lists = [Some(submit_list)];
        unsafe { queue.ExecuteCommandLists(&cmd_lists[..]) };

        unsafe { swapchain.Present(1, 0)? };

        backbuffer.fence_value += 1;
        frame_index = unsafe { swapchain.GetCurrentBackBufferIndex() };
        unsafe { queue.Signal(&fence, backbuffers[frame_index as usize].fence_value)? };

    }

    Ok(())
}
