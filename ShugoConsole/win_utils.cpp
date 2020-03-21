#include "win_utils.hpp"

#include <algorithm>

#include <Psapi.h>

namespace shugoconsole::WinUtils
{
std::filesystem::path GetModulePath(HMODULE hModule)
{
	std::wstring exePath(MAX_PATH, L'\0');
	const size_t charCount = ::GetModuleFileNameExW(
		::GetCurrentProcess(), hModule, exePath.data(), static_cast<DWORD>(exePath.size()));
	if (charCount > MAX_PATH)
	{
		exePath.resize(charCount);
		::GetModuleFileNameExW(::GetCurrentProcess(), hModule, exePath.data(), static_cast<DWORD>(exePath.size()));
	}
	// trim trailing null characters
	exePath.erase(
		std::find_if(exePath.rbegin(), exePath.rend(), [](wchar_t c) { return c == L'\0'; }).base(),
		exePath.end());
	return exePath;
}

std::string GetLastErrorAsString()
{
	const auto errorMessageId = ::GetLastError();
	if (errorMessageId == 0)
		return {}; // No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = ::FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorMessageId,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&messageBuffer,
		0,
		NULL);

	std::string message{messageBuffer, size};

	// Free the buffer.
	::LocalFree(messageBuffer);

	return message;
}

HMODULE GetCurrentModuleHandle()
{
	HMODULE hModule = nullptr;
	::GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)GetCurrentModuleHandle, &hModule);
	return hModule;
}
} // namespace shugoconsole::WinUtils
