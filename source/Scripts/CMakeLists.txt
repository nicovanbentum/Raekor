cmake_minimum_required(VERSION 3.15)

project(Scripts C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Rule to build the actual exectuable and enable c++17
add_library(${PROJECT_NAME} SHARED ${CppFiles} ${HppFiles})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Link all the VCPKG things
# target_link_libraries(${PROJECT_NAME} PRIVATE Raekor)

# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/dependencies
	${CMAKE_SOURCE_DIR}/dependencies/stb
    ${CMAKE_SOURCE_DIR}/dependencies/cgltf
    ${CMAKE_SOURCE_DIR}/dependencies/imnodes
    ${CMAKE_SOURCE_DIR}/dependencies/glm/glm
    ${CMAKE_SOURCE_DIR}/dependencies/entt/src
    ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo
    ${CMAKE_SOURCE_DIR}/dependencies/cereal/include
    ${CMAKE_SOURCE_DIR}/dependencies/JoltPhysics/Jolt
    ${CMAKE_SOURCE_DIR}/dependencies/IconFontCppHeaders
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)