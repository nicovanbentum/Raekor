cmake_minimum_required(VERSION 3.15)

project(Editor C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h ${Editor_SOURCE_DIR}/*Widget.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp ${Editor_SOURCE_DIR}/*Widget.cpp)

# Rule to build the actual exectuable and enable c++20
add_executable(${PROJECT_NAME} ${CppFiles} ${HppFiles} Assets/Icon/Icon.rc)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Editor")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine)
target_link_libraries(${PROJECT_NAME} PRIVATE Scripts)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/xess/bin/libxess.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/dev/nvngx_dlss.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/dxc/dxil.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/dxc/dxcompiler.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage/dstorage.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage/dstoragecore.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/Agility-SDK/D3D12Core.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/Agility-SDK/d3d12SDKLayers.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/PIX/bin/x64/WinPixEventRuntime.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Code
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
)