#ifndef SHUGOCONSOLE_WIN_FILE_MONITOR_HPP
#define SHUGOCONSOLE_WIN_FILE_MONITOR_HPP

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <type_traits>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole::win
{

class file_monitor
{
public:
	file_monitor(
		std::filesystem::path file_path,
		std::chrono::steady_clock::duration interval);

	HANDLE event_handle() const;
	bool changed() const;
	void reset();
	void on_event_signaled();

private:
	bool changed_ = false;
	std::filesystem::path file_path_;
	std::unique_ptr<
		std::remove_pointer_t<HANDLE>,
		decltype(&::FindCloseChangeNotification)>
		change_event_;
	std::chrono::steady_clock::time_point last_time_point_;
	std::optional<std::filesystem::file_time_type> last_write_;
	std::chrono::steady_clock::duration interval_;
};

} // namespace shugoconsole::win

#endif // SHUGOCONSOLE_WIN_FILE_MONITOR_HPP
