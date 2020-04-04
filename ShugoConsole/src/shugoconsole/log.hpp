#ifndef SHUGOCONSOLE_LOG_HPP
#define SHUGOCONSOLE_LOG_HPP

#include <spdlog/spdlog.h>

namespace shugoconsole::log
{

using spdlog::critical;
using spdlog::debug;
using spdlog::error;
using spdlog::info;
using spdlog::trace;
using spdlog::warn;

// Setups a log file in %TMP%/ShugoConsole
void setup_logger();

} // namespace shugoconsole::log

#endif // SHUGOCONSOLE_LOGGER_HPP
