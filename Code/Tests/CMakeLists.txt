cmake_minimum_required(VERSION 3.15)

project(Tests C CXX)

file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${CppFiles} ${HppFiles})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Code/Engine/PCH.h)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Code
    ${CMAKE_SOURCE_DIR}/Code/Engine
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
)
