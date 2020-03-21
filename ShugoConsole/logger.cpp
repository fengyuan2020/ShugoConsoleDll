#include "logger.hpp"

#include <filesystem>

#include <spdlog/sinks/basic_file_sink.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole::log
{
void setup_logger()
{
	const auto log_dir = std::filesystem::temp_directory_path() / L"shugoconsole";

	std::error_code ec;
	std::filesystem::create_directories(log_dir, ec);

	const auto log_name =
		fmt::format(L"shugoconsole_{}.log", ::GetProcessId(::GetCurrentProcess()));
	const auto log_path = log_dir / log_name;

	const auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		std::filesystem::path::string_type{log_path.native()}, true);
	const auto logger = std::make_shared<spdlog::logger>("logger", file_sink);

	// Always flush because we can't do it on exit
	logger->flush_on(spdlog::level::trace);

	spdlog::set_default_logger(logger);
	spdlog::set_level(spdlog::level::trace);
}
} // namespace shugoconsole::Logger
