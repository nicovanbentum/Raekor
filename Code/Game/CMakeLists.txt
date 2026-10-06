cmake_minimum_required(VERSION 3.15)

# SCRIPTS LIBRARY

project(Scripts C CXX)

# Build a list of all cpp source files
file(GLOB_RECURSE ScriptCppFiles CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/Scripts/*.cpp)
file(GLOB_RECURSE ScriptHppFiles CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/Scripts/*.h)

# Rule to build the hot loadable scripts DLL and enable c++20
add_library(Scripts SHARED ${ScriptCppFiles} ${ScriptHppFiles})
target_compile_features(Scripts PUBLIC cxx_std_20)
target_link_options(Scripts PRIVATE "/PDBALTPATH:%_PDB%")

# MSVC stuff for root working directory
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_link_libraries(Scripts PRIVATE Engine)

# GAME EXECUTABLE

project(Game C CXX)

# Build a list of all cpp source files
file(GLOB HppFiles ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB CppFiles ${PROJECT_SOURCE_DIR}/*.cpp)

# Rule to build the actual exectuable and enable c++20
add_executable(Game ${CppFiles} ${HppFiles})
target_compile_features(Game PUBLIC cxx_std_20)

# MSVC stuff for root working directory
set_property(TARGET Game PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Link all the things
target_link_libraries(Game PRIVATE Engine)
add_dependencies(Game Scripts)
