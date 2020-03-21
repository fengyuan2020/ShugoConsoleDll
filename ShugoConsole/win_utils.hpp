#ifndef SHUGOCONSOLE_WIN_UTILS_HPP
#define SHUGOCONSOLE_WIN_UTILS_HPP

#include <filesystem>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole::WinUtils
{
// Returns the path to the module file
std::filesystem::path GetModulePath(HMODULE hModule);
// Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString();
// Returns the current module handle and increments its reference counter
HMODULE GetCurrentModuleHandle();
} // namespace shugoconsole::WinUtils

#endif // SHUGOCONSOLE_WIN_UTILS_HPP
