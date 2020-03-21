add_library(detours STATIC)
target_include_directories(detours
	PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/Detours/src"
)
target_sources(detours
	PUBLIC
	Detours/src/detours.h
	Detours/src/detver.h
	PRIVATE
	Detours/src/creatwth.cpp
	Detours/src/detours.cpp
	Detours/src/disasm.cpp
	Detours/src/image.cpp
	Detours/src/modules.cpp
)
target_compile_options(detours
	PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-Wall>
	$<$<CXX_COMPILER_ID:MSVC>:/W4>
)
add_library(detours::detours ALIAS detours)
