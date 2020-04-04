#ifndef SHUGOCONSOLE_CRY_MEMORY_HPP
#define SHUGOCONSOLE_CRY_MEMORY_HPP

#include <cstddef>
#include <vector>

#include "shugoconsole/cry/cvar.hpp"

namespace shugoconsole::cry
{
cvar* find_cvar_ptr(
	const cvar::pattern& cvarDef, std::vector<std::byte>& buffer);
}

#endif // SHUGOCONSOLE_CRY_MEMORY_HPP
