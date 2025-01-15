cmake_minimum_required(VERSION 3.15)

project(Scripts C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Rule to build the actual exectuable and enable c++20
add_library(${PROJECT_NAME} SHARED ${CppFiles} ${HppFiles})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_library(DISCORD_LIB NAMES discord_game_sdk lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/lib/x86_64 NO_DEFAULT_PATH REQUIRED)

# Link all the VCPKG things
target_link_libraries(${PROJECT_NAME} PRIVATE Engine)
target_link_libraries(${PROJECT_NAME} PRIVATE debug ${DISCORD_LIB} optimized ${DISCORD_LIB})



# include dependency directories 
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_SOURCE_DIR}/source
    ${CMAKE_SOURCE_DIR}/ThirdParty
	${CMAKE_SOURCE_DIR}/ThirdParty/stb
    ${CMAKE_SOURCE_DIR}/ThirdParty/ufbx
    ${CMAKE_SOURCE_DIR}/ThirdParty/cgltf
    ${CMAKE_SOURCE_DIR}/ThirdParty/imnodes
    ${CMAKE_SOURCE_DIR}/ThirdParty/glm/glm
    ${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo
    ${CMAKE_SOURCE_DIR}/ThirdParty/JoltPhysics
	${CMAKE_SOURCE_DIR}/ThirdParty/BinaryRelations
    ${CMAKE_SOURCE_DIR}/ThirdParty/IconFontCppHeaders
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)