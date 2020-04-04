#include "shugoconsole/win/file_monitor.hpp"

namespace shugoconsole::win
{

static std::optional<std::filesystem::file_time_type>
opt_last_write_time(std::filesystem::path file_path)
{
	std::error_code ec;
	std::optional<std::filesystem::file_time_type> last_write =
		std::filesystem::last_write_time(file_path, ec);
	if (ec)
		last_write.reset();
	return last_write;
}

file_monitor::file_monitor(
	std::filesystem::path file_path,
	std::chrono::steady_clock::duration interval) :
	file_path_{std::move(file_path)},
	change_event_{
		::FindFirstChangeNotificationW(
			file_path_.parent_path().c_str(),
			FALSE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE),
		&::FindCloseChangeNotification},
	last_time_point_{std::chrono::steady_clock::now()},
	last_write_{opt_last_write_time(file_path_)},
	interval_{interval}
{
}

HANDLE file_monitor::event_handle() const
{
	return change_event_.get();
}

bool file_monitor::changed() const
{
	return changed_ &&
		   std::chrono::steady_clock::now() - last_time_point_ > interval_;
}

void file_monitor::reset()
{
	changed_ = false;
}

void file_monitor::on_event_signaled()
{
	::FindNextChangeNotification(change_event_.get());

	const auto new_last_write = opt_last_write_time(file_path_);
	if (new_last_write != last_write_)
	{
		last_time_point_ = std::chrono::steady_clock::now();
		last_write_ = new_last_write;
		changed_ = true;
	}
}

} // namespace shugoconsole::win
