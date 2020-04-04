#include "shugoconsole/cry/memory.hpp"
#include "shugoconsole/log.hpp"
#include "shugoconsole/win/utils.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole::cry
{

static cry::cvar* LookupPage(
	const MEMORY_BASIC_INFORMATION& memoryBasicInformation,
	const cry::cvar::pattern& cvarDef,
	std::vector<std::byte>& buffer)
{
	std::byte* currentReadAddress =
		static_cast<std::byte*>(memoryBasicInformation.BaseAddress);
	size_t remainingBytesInRegion = memoryBasicInformation.RegionSize;

	for (;;)
	{
		const size_t currentBufferSize =
			std::min(buffer.size(), remainingBytesInRegion);
		SIZE_T bytesRead = 0;

		if (!::ReadProcessMemory(
				::GetCurrentProcess(),
				currentReadAddress,
				buffer.data(),
				currentBufferSize,
				&bytesRead))
		{
			log::debug(
				"Could not read {:d} bytes at address {}, stopping region "
				"scan: {}",
				currentBufferSize,
				static_cast<void*>(currentReadAddress),
				win::get_last_error_as_string());
			break;
		}

		if (bytesRead == 0)
		{
			log::debug(
				"Read 0 bytes at address {}, stopping region scan",
				static_cast<void*>(currentReadAddress));
			break;
		}

		// Stop reading buffer before we have an incomplete var_name
		size_t endOffset = bytesRead - cvarDef.size();

		// CryCVar structs are aligned on multiples of 16 bytes inside pages
		for (size_t offset = 0; offset < endOffset; offset += 16)
		{
			// It's ok to alias to CryEngineCVar* here since g_LookupBuffer is
			// buffer of std::byte and this type is an exception to the C/C++
			// aliasing rules
			const auto cvarInBuffer = static_cast<cry::cvar*>(
				static_cast<void*>(buffer.data() + offset));
			if (cvarDef.match(cvarInBuffer))
			{
				const auto cvarReadAddress = static_cast<cry::cvar*>(
					static_cast<void*>(currentReadAddress + offset));
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

cry::cvar* find_cvar_ptr(
	const cry::cvar::pattern& cvarDef,
	std::vector<std::byte>& buffer)
{
	const std::byte* readAddress = nullptr;
	MEMORY_BASIC_INFORMATION memoryBasicInformation{};
	cry::cvar* cvarPtr = nullptr;

	log::trace(
		"find_cvar_ptr: Start of memory scan for variable {}",
		cvarDef.name());

	for (;;)
	{
		log::trace(
			"Calling VirtualQueryEx with base address {}",
			static_cast<const void*>(readAddress));

		if (!::VirtualQueryEx(
				::GetCurrentProcess(),
				readAddress,
				&memoryBasicInformation,
				sizeof(MEMORY_BASIC_INFORMATION)))
		{
			log::debug(
				"VirtualQueryEx failed: {}",
				win::get_last_error_as_string());
			break;
		}

		if (memoryBasicInformation.Type == MEM_PRIVATE &&
			memoryBasicInformation.State == MEM_COMMIT &&
			(memoryBasicInformation.Protect == PAGE_READWRITE ||
			 memoryBasicInformation.Protect == PAGE_EXECUTE_READWRITE))
		{
			log::trace(
				"Candidate region at: {} - {:d} bytes - Scanning...",
				static_cast<void*>(memoryBasicInformation.BaseAddress),
				memoryBasicInformation.RegionSize);

			cvarPtr = LookupPage(memoryBasicInformation, cvarDef, buffer);

			if (cvarPtr)
			{
				log::trace("Found CryEngine CVar!");
				break;
			}
		}
		else
		{
			log::trace(
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
			log::trace("Reached end of user-mode address space!");
			break;
		}
		else if (nextAddress <= readAddress)
		{
			log::trace("nextAddress <= readAddress - wtf!");
			break;
		}
		else
		{
			readAddress = nextAddress;
		}
	}

	log::trace("find_cvar_ptr: End of memory scan");

	return cvarPtr;
}

} // namespace shugoconsole
