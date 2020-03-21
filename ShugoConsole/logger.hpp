#ifndef SHUGOCONSOLE_LOGGER_HPP
#define SHUGOCONSOLE_LOGGER_HPP

#include <spdlog/spdlog.h>

namespace shugoconsole::log
{
using spdlog::critical;
using spdlog::debug;
using spdlog::error;
using spdlog::info;
using spdlog::trace;
using spdlog::warn;

void setup_logger();

} // namespace shugoconsole::Logger

#endif // SHUGOCONSOLE_LOGGER_HPP
