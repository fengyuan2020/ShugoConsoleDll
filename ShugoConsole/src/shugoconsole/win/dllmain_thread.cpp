#include "shugoconsole/win/dllmain_thread.hpp"
#include "shugoconsole/log.hpp"

namespace shugoconsole::win
{
dllmain_thread::dllmain_thread(dllmain_thread&& other) noexcept
{
	*this = std::move(other);
}

dllmain_thread& dllmain_thread::operator=(dllmain_thread&& other) noexcept
{
	if (thread_)
		std::terminate();

	func_ = std::move(other.func_);
	std::swap(module_, other.module_);
	std::swap(quit_event_, other.quit_event_);
	std::swap(thread_, other.thread_);

	return *this;
}

dllmain_thread::~dllmain_thread()
{
	if (thread_)
	{
		// When called from DllMain's DLL_PROCESS_DETACH,
		// In case of TerminateProcess, try exitting the thread gracefully
		// In case of ExitProcess, the thread will be stopped forcefully
		// and thread_ will be in the signalled state
		::SetEvent(quit_event_);
		::WaitForSingleObject(thread_, INFINITE);

		::CloseHandle(quit_event_);
		::CloseHandle(thread_);
	}
}

void dllmain_thread::start()
{
	// Increment current module reference count
	::GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPCTSTR)&dllmain_thread::entry_point,
		&module_);

	quit_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

	DWORD dwThreadId{};
	thread_ = ::CreateThread(
		nullptr,
		0,
		&dllmain_thread::entry_point,
		nullptr,
		0,
		&dwThreadId);
}

[[noreturn]] void dllmain_thread::run()
{
	try
	{
		(*func_)();
	}
	catch (std::exception& e)
	{
		log::critical("Uncaught std::exception: {}", e.what());
	}
	catch (...)
	{
		log::critical("Uncaught unknown exception!");
	}

	log::debug("Waiting for thread quit event...");
	::WaitForSingleObject(quit_event_, INFINITE);

	::FreeLibraryAndExitThread(module_, 0);
}

DWORD CALLBACK dllmain_thread::entry_point(void* this_ptr)
{
	static_cast<dllmain_thread*>(this_ptr)->run();
}

} // namespace shugoconsole::win
