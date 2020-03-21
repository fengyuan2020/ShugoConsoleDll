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

#######################################################################
# ShugoConsole Dll
#######################################################################

add_library(ShugoConsoleDll SHARED)

#
# Auto generate DLL export header
#
include(GenerateExportHeader)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(ShugoConsoleDll_EXPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(ShugoConsoleDll_EXPORT_FILE_NAME "${ShugoConsoleDll_EXPORT_DIR}/shugoconsole_export.hpp")
generate_export_header(ShugoConsoleDll
	BASE_NAME "ShugoConsole"
	EXPORT_MACRO_NAME "SHUGOCONSOLE_EXPORT"
	EXPORT_FILE_NAME "${ShugoConsoleDll_EXPORT_FILE_NAME}")
target_include_directories(ShugoConsoleDll PUBLIC "${ShugoConsoleDll_EXPORT_DIR}")

# target Windows 7 function calls
target_compile_definitions(ShugoConsoleDll PRIVATE -D_WIN32_WINNT=0x0601)

target_sources(ShugoConsoleDll
	PRIVATE
	shugoconsole.cpp
	shugoconsole.hpp
	"${ShugoConsoleDll_EXPORT_FILE_NAME}"
	shugoconsole.def)

set_target_properties(ShugoConsoleDll
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	OUTPUT_NAME "ShugoConsole"
)

target_link_libraries(ShugoConsoleDll
	PRIVATE
	fmt::fmt
	spdlog::spdlog
	toml11::toml11
	ntdll.lib)

#######################################################################
# Test executable
#######################################################################

add_executable(DllRunner)
target_sources(DllRunner PRIVATE dllrunner.cpp)
target_link_libraries(DllRunner PRIVATE ShugoConsoleDll)
set_target_properties(DllRunner
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
)
