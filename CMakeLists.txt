cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(ShugoConsoleDll
	VERSION 0.1.0
	DESCRIPTION "Dll-based version of ShugoConsole"
	LANGUAGES CXX)

#######################################################################
# Global variables
#######################################################################

# Force using static runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/bin")

#######################################################################
# Dependencies
#######################################################################

# fmt
add_subdirectory(libs/fmt)

# spdlog
set(SPDLOG_WCHAR_FILENAMES ON CACHE BOOL "Support wchar filenames")
add_subdirectory(libs/spdlog)

# toml11
set(toml11_BUILD_TEST OFF CACHE BOOL "Build toml tests")
add_subdirectory(libs/toml11)

# detours
add_subdirectory(libs/detours_cmake)

# outcome
add_subdirectory(libs/outcome)

#######################################################################
# Projects
#######################################################################

add_subdirectory(ShugoConsole)
add_subdirectory(Aion-Version-Dll)
