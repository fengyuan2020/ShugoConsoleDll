#ifndef SHUGOCONSOLE_WIN_UTILS_HPP
#define SHUGOCONSOLE_WIN_UTILS_HPP

#include <chrono>
#include <filesystem>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole::win
{

// Returns the path to %APPDATA%
std::filesystem::path get_appdata_path();

// Wrapper around Windows' GetLastError() function
std::string get_last_error_as_string();

// Simple wrapper around Windows' WaitForMultipleObjects
// Takes any convertible std::chrono::duration and an arbitrary
// number of handles to wait on
// Returns on error, timeout, or as soon as one of the handles
// is in signalled state
template<typename ...Args>
DWORD wait_on_objects(
	std::chrono::duration<DWORD, std::milli> wait_duration,
	Args... args)
{
	static_assert(sizeof...(Args) >= 1,
		"At least one HANDLE must be passed");
	static_assert(
		std::conjunction_v<std::is_same<HANDLE, Args>...>,
		"All variadic arguments must be of HANDLE type");

	std::array<HANDLE, sizeof...(Args)> handles{args...};

	return ::WaitForMultipleObjects(
		static_cast<DWORD>(handles.size()),
		handles.data(),
		FALSE,
		wait_duration.count());
}

} // namespace shugoconsole::win

#endif // SHUGOCONSOLE_WIN_UTILS_HPP
