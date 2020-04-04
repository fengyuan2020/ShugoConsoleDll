#include "shugoconsole/win/utils.hpp"

#include <algorithm>

#include <Shlobj.h>

namespace shugoconsole::win
{

std::filesystem::path get_appdata_path()
{
	std::filesystem::path ret;

	PWSTR path = nullptr;
	if (SUCCEEDED(::SHGetKnownFolderPath(
			::FOLDERID_RoamingAppData,
			::KF_FLAG_DEFAULT,
			nullptr,
			&path)))
	{
		ret = path;
		::CoTaskMemFree(path);
	}

	return ret;
}

std::string get_last_error_as_string()
{
	const auto errorMessageId = ::GetLastError();
	if (errorMessageId == 0)
		return {}; // No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = ::FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
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

} // namespace shugoconsole::win
