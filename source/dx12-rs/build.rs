use std::{path::{PathBuf}, error::Error, env};

fn main() -> Result<(), Box<dyn Error>> {
    let workspace_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let profile = env::var("PROFILE").unwrap();

    let built_exe_dir = workspace_dir.join("target").join(profile);
    let deps_dir = workspace_dir.join("../../dependencies/");
    let agility_sdk_dir = workspace_dir.join("../../dependencies/Agility-SDK/");

    let agility_sdk_files = [
        "D3D12Core.dll",
        "D3D12Core.pdb",
        "d3d12SDKLayers.dll",
        "d3d12SDKLayers.pdb",
    ];

    for file in agility_sdk_files {
        let d3d12_dir = built_exe_dir.join("D3D12");

        if !d3d12_dir.exists() {
            std::fs::create_dir(&d3d12_dir).expect("Failed to create D3D12 directory.");
        }

        println!("{}", d3d12_dir.to_str().unwrap());

        std::fs::copy(agility_sdk_dir.join(file), d3d12_dir.join(file)).expect("failed to copy agility-sdk files");
    }

    std::fs::copy(deps_dir.join("dxc/dxcompiler.dll"), built_exe_dir.join("dxcompiler.dll"))?;
    std::fs::copy(deps_dir.join("dxc/dxil.dll"), built_exe_dir.join("dxil.dll"))?;

    println!(
        "cargo:rustc-link-search={}",
        agility_sdk_dir
            .to_str()
            .unwrap()
    );

    println!("cargo:rustc-link-lib=dxgi");
    println!("cargo:rustc-link-lib=d3d12");
    println!(
        "cargo:rustc-link-arg=/DEF:{}\\agility.def",
        agility_sdk_dir
            .to_str()
            .unwrap()
    );

    Ok(())
}