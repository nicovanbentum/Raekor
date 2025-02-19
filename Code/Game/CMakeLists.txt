cmake_minimum_required(VERSION 3.15)

# SCRIPTS LIBRARY

project(Scripts C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE ScriptCppFiles ${PROJECT_SOURCE_DIR}/Scripts/*.cpp)

# Rule to build the actual exectuable and enable c++20
add_library(Scripts ${ScriptCppFiles})
target_compile_features(Scripts PUBLIC cxx_std_20)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_library(DISCORD_LIB NAMES discord_game_sdk lib PATHS ${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/lib/x86_64 NO_DEFAULT_PATH REQUIRED)

# Link all the VCPKG things
target_link_libraries(Scripts PRIVATE Engine)
target_link_libraries(Scripts PRIVATE debug ${DISCORD_LIB} optimized ${DISCORD_LIB})

# include dependency directories 
target_include_directories(Scripts PUBLIC 
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

# GAME EXECUTABLE

project(Game C CXX)

# Build a list of all cpp source files
file(GLOB HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Rule to build the actual exectuable and enable c++20
add_executable(Game ${CppFiles} ${HppFiles})
target_compile_features(Game PUBLIC cxx_std_20)

# MSVC stuff for root working directory, static runtime and enabling precompiled headers
set_property(TARGET Game PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET Game PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Link all the things
target_link_libraries(Game PRIVATE Engine)
target_link_libraries(Game PRIVATE Scripts)

add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/xess/bin/libxess.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DLSS/lib/Windows_x86_64/dev/nvngx_dlss.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/dxc/dxil.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/dxc/dxcompiler.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage/dstorage.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/DirectStorage/dstoragecore.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/Agility-SDK/D3D12Core.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/Agility-SDK/d3d12SDKLayers.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/discord_game_sdk/lib/x86_64/discord_game_sdk.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET Game POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/ThirdParty/PIX/bin/x64/WinPixEventRuntime.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# include dependency directories 
target_include_directories(Game PUBLIC 
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

