use std::default;

use bevy_ecs::prelude::*;
use glam::*;
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Debug, Component)]
pub struct Transform {
    scale : Vec3,
    position : Vec3,
    rotation : Quat,
    local_transform : Mat4,
    world_transform : Mat4
}

#[derive(Serialize, Deserialize, Debug, Component, Default)]
pub struct Mesh {
    pub positions : Vec<Vec3>,
    pub attributes : Vec<f32>,
    pub indices : Vec<u32>,
    pub vertex_buffer : u32,
    pub index_buffer : u32,
    pub aabb : [Vec3; 2],
    pub material : u32,
}

#[derive(Serialize, Deserialize, Debug, Component, PartialEq)]
pub struct Material {
    pub albedo : Vec4,
    pub emissive : Vec3,
    pub metallic : f32,
    pub roughness : f32,
    pub alpha_mask : bool,
    
    pub albedo_file : Option<String>,
    pub normals_file : Option<String>,
    pub material_file : Option<String>,

    pub gpu_albedo : u32,
    pub gpu_normal : u32,
    pub gpu_material : u32,
}

impl Material {
    pub fn new(material : &gltf::Material) -> Self {
        let pbr = &material.pbr_metallic_roughness();

        Self {
            albedo : Vec4::from_slice(&pbr.base_color_factor()),
            emissive : Vec3::from_slice(&material.emissive_factor()),
            metallic : pbr.metallic_factor(),
            roughness : pbr.roughness_factor(),
            alpha_mask : false,
            albedo_file : pbr.base_color_texture().and_then(| info | { 
                match info.texture().source().source() {
                    gltf::image::Source::Uri{uri, ..} => Some(String::from(uri)),
                    gltf::image::Source::View{..} => None
                }
            }),
            normals_file : material.normal_texture().and_then(| info | { 
                match info.texture().source().source() {
                    gltf::image::Source::Uri{uri, ..} => Some(String::from(uri)),
                    gltf::image::Source::View{..} => None
                }
            }),
            material_file : pbr.metallic_roughness_texture().and_then(| info | { 
                match info.texture().source().source() {
                    gltf::image::Source::Uri{uri, ..} => Some(String::from(uri)),
                    gltf::image::Source::View{..} => None
                }
            }),
            gpu_albedo : 0,
            gpu_normal : 0,
            gpu_material : 0
        }
    }
}