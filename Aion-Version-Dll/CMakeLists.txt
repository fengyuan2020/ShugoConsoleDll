add_library(Aion-Version-Dll SHARED)
target_sources(Aion-Version-Dll
	PRIVATE
	exports.cpp
	version.cpp
	version.def
	version.rc
)
target_compile_definitions(Aion-Version-Dll
	PRIVATE
	-D_WIN32_WINNT=0x0601
	-DUNICODE
)
target_link_libraries(Aion-Version-Dll
	PRIVATE
	ShugoConsole
	detours::detours
	wsock32
	ws2_32
)
set_target_properties(Aion-Version-Dll
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	OUTPUT_NAME "version"
)

