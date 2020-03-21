#ifndef SHUGOCONSOLE_MEMORY_PARSER_HPP
#define SHUGOCONSOLE_MEMORY_PARSER_HPP

#include <cstddef>
#include <vector>

#include "cryengine_cvar.hpp"

namespace shugoconsole
{
cryengine::cvar* find_cvar_ptr(
	const cryengine::cvar_pattern& cvarDef, std::vector<std::byte>& buffer);
}

#endif // SHUGOCONSOLE_MEMORY_PARSER_HPP
