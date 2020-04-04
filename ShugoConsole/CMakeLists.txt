#######################################################################
# ShugoConsole static library
#######################################################################

add_library(ShugoConsole STATIC)
target_include_directories(ShugoConsole
	PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
)
target_sources(ShugoConsole
	PRIVATE
	src/shugoconsole/shugoconsole.cpp
	src/shugoconsole/shugoconsole.hpp
	src/shugoconsole/cry/cvar.cpp
	src/shugoconsole/cry/cvar.hpp
	src/shugoconsole/cry/memory.cpp
	src/shugoconsole/cry/memory.hpp
	src/shugoconsole/win/utils.cpp
	src/shugoconsole/win/utils.hpp
	src/shugoconsole/win/file_monitor.cpp
	src/shugoconsole/win/file_monitor.hpp
	src/shugoconsole/win/dllmain_thread.cpp
	src/shugoconsole/win/dllmain_thread.hpp
	src/shugoconsole/log.cpp
	src/shugoconsole/log.hpp
	src/shugoconsole/config.cpp
	src/shugoconsole/config.hpp
)

target_include_directories(ShugoConsole PUBLIC src)

target_compile_definitions(ShugoConsole
	PRIVATE
	-D_WIN32_WINNT=0x0601
	-DUNICODE
)
target_compile_options(ShugoConsole
	PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall>
	$<$<CXX_COMPILER_ID:MSVC>:/W4>
)
set_target_properties(ShugoConsole
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
)
target_link_libraries(ShugoConsole
	PRIVATE
	fmt::fmt
	spdlog::spdlog
	toml11::toml11
	outcome::outcome
)

#######################################################################
# Test executable
#######################################################################

add_executable(TestShugoConsole)
target_sources(TestShugoConsole
	PRIVATE
	src/test.cpp
)
target_link_libraries(TestShugoConsole
	PRIVATE
	ShugoConsole
)
set_target_properties(TestShugoConsole
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
)
