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
	shugoconsole.cpp
	shugoconsole.hpp
	cryengine_cvar.cpp
	cryengine_cvar.hpp
	win_utils.cpp
	win_utils.hpp
	memory_parser.cpp
	memory_parser.hpp
	logger.cpp
	logger.hpp
)
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
)

#######################################################################
# Test executable
#######################################################################

add_executable(TestShugoConsole)
target_sources(TestShugoConsole
	PRIVATE
	test.cpp
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
