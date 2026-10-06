cmake_minimum_required(VERSION 3.15)

project(Editor C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h ${Editor_SOURCE_DIR}/*Widget.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp ${Editor_SOURCE_DIR}/*Widget.cpp)

# Rule to build the actual exectuable and enable c++20
add_executable(${PROJECT_NAME} ${CppFiles} ${HppFiles} Assets/Icon/Icon.rc)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Editor")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# MSVC stuff for root working directory and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine)
add_dependencies(${PROJECT_NAME} Scripts)

target_compile_definitions(${PROJECT_NAME} PRIVATE RK_CMAKE_COMMAND="${CMAKE_COMMAND}" RK_BINARY_DIR="${CMAKE_BINARY_DIR}" RK_BUILD_CONFIG="$<CONFIG>")

# include dependency directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Code
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
)
