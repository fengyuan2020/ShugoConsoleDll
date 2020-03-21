#include "shugoconsole.hpp"

// Standard library includes
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>

// Third-party libraries
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <toml.hpp>

// Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Psapi.h>
#include <Windows.h>

static HMODULE g_ModuleInstance = nullptr;
static HANDLE g_QuitEvent = nullptr;
static HANDLE g_Thread = nullptr;

namespace fs = std::filesystem;

// Returns the path to
static fs::path GetDllPath()
{
	std::wstring exePath(MAX_PATH, L'\0');
	const size_t charCount = ::GetModuleFileNameExW(
		::GetCurrentProcess(), g_ModuleInstance, exePath.data(), exePath.size());
	if (charCount > MAX_PATH)
	{
		exePath.resize(charCount);
		::GetModuleFileNameExW(
			::GetCurrentProcess(), g_ModuleInstance, exePath.data(), exePath.size());
	}
	// trim trailing null characters
	exePath.erase(
		std::find_if(exePath.rbegin(), exePath.rend(), [](wchar_t c) { return c == L'\0'; }).base(),
		exePath.end());
	return exePath;
}

// Returns the last Win32 error, in string format. Returns an empty string if there is no error.
static std::string GetLastErrorAsString()
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

using spdlog::critical;
using spdlog::debug;
using spdlog::error;
using spdlog::info;
using spdlog::trace;
using spdlog::warn;

void SetupLogger()
{
	const auto log_dir = fs::temp_directory_path() / L"shugoconsole";

	std::error_code ec;
	fs::create_directories(log_dir, ec);

	const auto log_name =
		fmt::format(L"shugoconsole_{}.log", ::GetProcessId(::GetCurrentProcess()));
	const auto log_path = log_dir / log_name;

	const auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		fs::path::string_type{log_path.native()}, true);
	const auto logger = std::make_shared<spdlog::logger>("logger", file_sink);

	// Always flush because we can't do it on exit
	logger->flush_on(spdlog::level::trace);

	spdlog::set_default_logger(logger);
	spdlog::set_level(spdlog::level::trace);

	info("===== ShugoConsole =====");
}

// Aion's CryEngine CVar memory model
// Dummy member types are probably not accurate but they model the gap
// between name and int_value by abusing memory alignment
struct CryEngineCVar final
{
	//                         X86 offset | X86 align | X64 offset | X64 align
	void* dummy0;           // 0          | 4         | 0          | 8
	char cat;               // 4          | 1         | 8          | 1
	char name[128];         // 5          | 1         | 9          | 1
	void* dummy1;           // 133 >> 136 | 4         | 137 >> 144 | 8
	int dummy2;             // 140        | 4         | 152        | 4
	int dummy3;             // 144        | 4         | 156        | 4
	int dummy4;             // 148        | 4         | 160        | 4
	void* dummy5;           // 152        | 4         | 164 >> 168 | 8
	void* dummy6;           // 156        | 4         | 176        | 8
	int int_value;          // 160        | 4         | 184        | 4
	float float_value;      // 164        | 4         | 188        | 4
	char string_value[256]; // 168        | 1         | 192        | 1
};

// Assert known offsets
#if defined _M_X64
static_assert(offsetof(CryEngineCVar, cat) == 8);
static_assert(offsetof(CryEngineCVar, name) == 9);
static_assert(offsetof(CryEngineCVar, int_value) == 184);
static_assert(offsetof(CryEngineCVar, float_value) == 188);
static_assert(offsetof(CryEngineCVar, string_value) == 192);
#elif defined _M_IX86
static_assert(offsetof(CryEngineCVar, cat) == 4);
static_assert(offsetof(CryEngineCVar, name) == 5);
static_assert(offsetof(CryEngineCVar, int_value) == 160);
static_assert(offsetof(CryEngineCVar, float_value) == 164);
static_assert(offsetof(CryEngineCVar, string_value) == 168);
#else
#	error "Unrecognized architecture"
#endif

class CryEngineCVarDefinition final
{
	std::string name_;

  public:
	CryEngineCVarDefinition(std::string name) noexcept : name_{std::move(name)} {}

	inline bool match(const CryEngineCVar& cvar) const noexcept
	{
		// Is a valid cvar if type is 0 or 1 and string matches,
		// including trailing null character
		return (cvar.cat == 0 || cvar.cat == 1) &&
			   (std::memcmp(name_.c_str(), cvar.name, name_.size() + 1) == 0);
	}

	// Minimum amount of bytes needed in a buffer to match a CryEngineCVarDefinition
	inline size_t size() const noexcept { return offsetof(CryEngineCVar, name) + name_.size() + 1; }
};

static std::vector<std::byte> g_LookupBuffer(64 * 1024);

static CryEngineCVar* LookupPage(
	const MEMORY_BASIC_INFORMATION& memoryBasicInformation,
	const CryEngineCVarDefinition& cvarDef) noexcept
{
	std::byte* currentReadAddress = static_cast<std::byte*>(memoryBasicInformation.BaseAddress);
	size_t remainingBytesInRegion = memoryBasicInformation.RegionSize;

	for (;;)
	{
		const size_t currentBufferSize = std::min(g_LookupBuffer.size(), remainingBytesInRegion);
		SIZE_T bytesRead = 0;

		if (!::ReadProcessMemory(
				::GetCurrentProcess(),
				currentReadAddress,
				g_LookupBuffer.data(),
				currentBufferSize,
				&bytesRead))
		{
			debug(
				"Could not read {:d} bytes at address {}, stopping region scan: {}",
				currentBufferSize,
				static_cast<void*>(currentReadAddress),
				GetLastErrorAsString());
			break;
		}

		if (bytesRead == 0)
		{
			debug(
				"Read 0 bytes at address {}, stopping region scan",
				static_cast<void*>(currentReadAddress));
			break;
		}

		// Stop reading buffer before we have an incomplete name
		size_t endOffset = bytesRead - cvarDef.size();

		// CryCVar structs are aligned on multiples of 16 bytes inside pages
		for (size_t offset = 0; offset < endOffset; offset += 16)
		{
			// It's ok to alias to CryEngineCVar* here since g_LookupBuffer is buffer of std::byte
			// and this type is an exception to the C/C++ aliasing rules
			const auto cvarInBuffer =
				static_cast<CryEngineCVar*>(static_cast<void*>(g_LookupBuffer.data() + offset));
			if (cvarDef.match(*cvarInBuffer))
			{
				const auto cvarReadAddress =
					static_cast<CryEngineCVar*>(static_cast<void*>(currentReadAddress + offset));
				return cvarReadAddress;
			}
		}

		if (bytesRead < remainingBytesInRegion)
		{
			remainingBytesInRegion -= bytesRead;
			currentReadAddress += bytesRead;
		}
		else
		{
			break;
		}
	}

	return nullptr;
}

static CryEngineCVar* GetAddressOfCryEngineCVar(const CryEngineCVarDefinition& cvarDef) noexcept
{
	const std::byte* readAddress = nullptr;
	MEMORY_BASIC_INFORMATION memoryBasicInformation{};
	CryEngineCVar* cvarPtr = nullptr;

	trace("GetAddressOfCryEngineCVar: Start of memory scan");

	for (;;)
	{
		trace("Calling VirtualQueryEx with base address {}", static_cast<const void*>(readAddress));

		if (!::VirtualQueryEx(
				::GetCurrentProcess(),
				readAddress,
				&memoryBasicInformation,
				sizeof(MEMORY_BASIC_INFORMATION)))
		{
			debug("VirtualQueryEx failed: {}", GetLastErrorAsString());
			break;
		}

		if (memoryBasicInformation.Type == MEM_PRIVATE &&
			memoryBasicInformation.State == MEM_COMMIT &&
			(memoryBasicInformation.Protect == PAGE_READWRITE ||
			 memoryBasicInformation.Protect == PAGE_EXECUTE_READWRITE))
		{
			trace(
				"Candidate region at: {} - {:d} bytes - Scanning...",
				static_cast<void*>(memoryBasicInformation.BaseAddress),
				memoryBasicInformation.RegionSize);

			cvarPtr = LookupPage(memoryBasicInformation, cvarDef);

			if (cvarPtr)
			{
				trace("Found CryEngine CVar!");
				break;
			}
		}
		else
		{
			trace(
				"Non-candidate region at: {} - {:d} bytes - Ignoring",
				static_cast<void*>(memoryBasicInformation.BaseAddress),
				memoryBasicInformation.RegionSize);
		}

		const std::byte* nextAddress =
			static_cast<const std::byte*>(memoryBasicInformation.BaseAddress) +
			memoryBasicInformation.RegionSize;

#if defined _M_X64
		constexpr uintptr_t VirtualMemoryMax = 0x7fffffff0000ull;
#elif defined _M_IX86
		constexpr uintptr_t VirtualMemoryMax = 0x78000000ul;
#else
#	error "Unrecognized architecture"
#endif

		if (nextAddress >= reinterpret_cast<const std::byte*>(VirtualMemoryMax))
		{
			trace("Reached end of user-mode address space!");
			break;
		}
		else if (nextAddress <= readAddress)
		{
			trace("nextAddress <= readAddress - wtf!");
			break;
		}
		else
		{
			readAddress = nextAddress;
		}
	}

	trace("GetAddressOfCryEngineCVar: End of memory scan");

	return cvarPtr;
}

// The dll must export at least one symbol to be added to an import table
extern "C" SHUGOCONSOLE_EXPORT void __stdcall Dummy()
{
}

static void ShugoConsole()
{
	info("Entered ShugoConsole thread!");

	const auto exePath = GetDllPath();
	info("Executable path: {}", exePath.u8string());

	const auto configPath = exePath.parent_path() / "shugoconsole.toml";
	info("Config file path: {}", configPath.u8string());

	// Use ifstream to open file because toml11 does not
	// support wchar_t filenames
	std::ifstream configStream{configPath.wstring()};
	if (!configStream.is_open())
		warn("Could not open configuration file!");

	const auto data = toml::parse(configStream);
	const double minFov = toml::find_or(data, "g_MinFov", 90.0);

	info("Variable values:");
	info("g_MinFov={}", minFov);

	GetAddressOfCryEngineCVar(CryEngineCVarDefinition{"g_minFov"});

	info("Waiting for Thread quit event...");
}

DWORD CALLBACK ShugoConsoleThread(void*)
{
	try
	{
		ShugoConsole();
	}
	catch (std::exception& e)
	{
		critical("Critical Exception: {}", e.what());
	}

	::WaitForSingleObject(g_QuitEvent, INFINITE);
	::FreeLibraryAndExitThread(g_ModuleInstance, 0);
}

BOOLEAN WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved)
{
	BOOLEAN bSuccess = TRUE;

	switch (nReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		// Increment DLL Reference Count
		::GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			reinterpret_cast<LPCWSTR>(hDllHandle),
			&g_ModuleInstance);

		// Setup logger
		SetupLogger();

		// Create Thread
		info("Creating ShugoConsole thread...");

		g_QuitEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
		DWORD dwThreadId = 0;
		g_Thread = ::CreateThread(nullptr, 0, &ShugoConsoleThread, nullptr, 0, &dwThreadId);
	}
	break;

	case DLL_PROCESS_DETACH:
		// In case of TerminateProcess, try exitting the thread gracefully
		// In case of ExitProcess, the thread will be stopped forcefully
		// and g_Thread will be in the signalled state
		::SetEvent(g_QuitEvent);
		::WaitForSingleObject(g_Thread, INFINITE);
		break;
	}

	return bSuccess;
}
