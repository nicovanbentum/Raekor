cmake_minimum_required(VERSION 3.15)

project(FSR2 C CXX)

add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCE_DIR}/FSR2.cpp ${PROJECT_SOURCE_DIR}/FSR2.def)
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_library(FSR2_LIBRARY NAMES ffx_fsr2_api_x64 PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)
find_library(FSR2_LIBRARY_DEBUG NAMES ffx_fsr2_api_x64d PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)
find_library(FSR2_DX12_LIBRARY NAMES ffx_fsr2_api_dx12_x64 PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)
find_library(FSR2_DX12_LIBRARY_DEBUG NAMES ffx_fsr2_api_dx12_x64d PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/lib NO_DEFAULT_PATH REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE debug ${FSR2_LIBRARY_DEBUG} optimized ${FSR2_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE debug ${FSR2_DX12_LIBRARY_DEBUG} optimized ${FSR2_DX12_LIBRARY})
target_link_libraries(${PROJECT_NAME} PRIVATE d3d12.lib dxgi.lib dxguid.lib)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/ThirdParty/FSR2/src)
