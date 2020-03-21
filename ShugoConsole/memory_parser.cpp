#include "memory_parser.hpp"
#include "logger.hpp"
#include "win_utils.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole
{
using namespace log;

static cryengine::cvar* LookupPage(
	const MEMORY_BASIC_INFORMATION& memoryBasicInformation,
	const cryengine::cvar_pattern& cvarDef,
	std::vector<std::byte>& buffer)
{
	std::byte* currentReadAddress = static_cast<std::byte*>(memoryBasicInformation.BaseAddress);
	size_t remainingBytesInRegion = memoryBasicInformation.RegionSize;

	for (;;)
	{
		const size_t currentBufferSize = std::min(buffer.size(), remainingBytesInRegion);
		SIZE_T bytesRead = 0;

		if (!::ReadProcessMemory(
				::GetCurrentProcess(),
				currentReadAddress,
				buffer.data(),
				currentBufferSize,
				&bytesRead))
		{
			debug(
				"Could not read {:d} bytes at address {}, stopping region scan: {}",
				currentBufferSize,
				static_cast<void*>(currentReadAddress),
				WinUtils::GetLastErrorAsString());
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
				static_cast<cryengine::cvar*>(static_cast<void*>(buffer.data() + offset));
			if (cvarDef.match(cvarInBuffer))
			{
				const auto cvarReadAddress =
					static_cast<cryengine::cvar*>(static_cast<void*>(currentReadAddress + offset));
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

cryengine::cvar* find_cvar_ptr(
	const cryengine::cvar_pattern& cvarDef, std::vector<std::byte>& buffer)
{
	const std::byte* readAddress = nullptr;
	MEMORY_BASIC_INFORMATION memoryBasicInformation{};
	cryengine::cvar* cvarPtr = nullptr;

	trace("find_cvar_ptr: Start of memory scan for variable {}", cvarDef.name());

	for (;;)
	{
		trace("Calling VirtualQueryEx with base address {}", static_cast<const void*>(readAddress));

		if (!::VirtualQueryEx(
				::GetCurrentProcess(),
				readAddress,
				&memoryBasicInformation,
				sizeof(MEMORY_BASIC_INFORMATION)))
		{
			debug("VirtualQueryEx failed: {}", WinUtils::GetLastErrorAsString());
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

			cvarPtr = LookupPage(memoryBasicInformation, cvarDef, buffer);

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

	trace("find_cvar_ptr: End of memory scan");

	return cvarPtr;
}

} // namespace shugoconsole
