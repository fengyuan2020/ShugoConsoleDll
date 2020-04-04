// Standard library includes
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <vector>

#include "shugoconsole/config.hpp"
#include "shugoconsole/cry/memory.hpp"
#include "shugoconsole/log.hpp"
#include "shugoconsole/shugoconsole.hpp"
#include "shugoconsole/win/dllmain_thread.hpp"
#include "shugoconsole/win/file_monitor.hpp"
#include "shugoconsole/win/utils.hpp"

// Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole
{

using namespace std::literals::chrono_literals;

const auto WAIT_TIME_AFTER_FILE_CHANGE = 1s;
const auto WAIT_TIME_AFTER_FAILED_SCAN = 2s;
const auto WAIT_TIME_AFTER_VAR_CHECK = 100ms;

const auto CONSOLE_VARS = config::configuration::variable_definition_set{
	{"g_minFov", config::types::floating::with_min_max(60.0, 170.0)},
	{"g_chatlog", config::types::boolean{}},
	{"g_camMax", config::types::floating::with_min_max(5.0f, 50.0f)},
	{"d3d9_TripleBuffering", config::types::boolean{}},
	{"g_maxfps", config::types::integer::with_min_max(0, 1000)},
	{"r_Texture_Anisotropic_Level",
	 config::types::integer::with_values({0, 2, 4, 8, 16})}};

class instance_impl final : public instance
{
public:
	instance_impl()
	{
		log::setup_logger();
		thread_ = win::dllmain_thread([this]() { run(); });
	}

private:
	win::dllmain_thread thread_;

	void run();

	struct console_var_task
	{
		config::configuration::variable cfg;
		cry::cvar* cvar;

		auto type() const { return cfg.def.cvar_type(); }
		auto name() const { return cfg.def.name; }
	};
};

instance::~instance() = default;

[[nodiscard]] std::unique_ptr<instance> create()
{
	return std::make_unique<instance_impl>();
}

void instance_impl::run()
{
	const auto configPath =
		win::get_appdata_path() / L"ShugoConsole" / "config.toml";
	log::info("Config file path: {}", configPath.u8string());

	// Monitor config file, wait 1 second before applying changes
	win::file_monitor configFileMonitor{configPath,
										WAIT_TIME_AFTER_FILE_CHANGE};

	auto cfg = config::configuration::from_file(CONSOLE_VARS, configPath);

	std::vector<console_var_task> console_var_tasks;

	std::transform(
		cfg.vars.begin(),
		cfg.vars.end(),
		std::back_inserter(console_var_tasks),
		[](const auto& cfg) {
			return console_var_task{cfg, nullptr};
		});

	// Find all configurable CVars in memory
	{
		std::vector<std::byte> buffer(64 * 1024);
		for (;;)
		{
			// Assuming that config_entries_ is not empty and that we test
			// all config variables after scanning memory, there is at least
			// one matching element in the vector
			auto& task = *std::find_if(
				console_var_tasks.begin(),
				console_var_tasks.end(),
				[](const console_var_task& e) { return e.cvar == nullptr; });

			task.cvar = cry::find_cvar_ptr({task.name()}, buffer);

			if (task.cvar)
			{
				log::info(
					"Found {} current={}",
					task.name(),
					cry::to_string(task.cvar->to_value(task.type())));
			}

			if (std::all_of(
					console_var_tasks.begin(),
					console_var_tasks.end(),
					[](const console_var_task& e) {
						return e.cvar != nullptr;
					}))
			{
				log::info("Found all configurable CVars !");
				break;
			}

			// Test if we have to quit the thread
			// In case we have not found the variable, wait before
			// continuing
			std::chrono::duration<DWORD, std::milli> waitTime =
				task.cvar ? 0ms : WAIT_TIME_AFTER_FAILED_SCAN;

			switch (win::wait_on_objects(waitTime, thread_.quit_event()))
			{
			case WAIT_OBJECT_0:
				log::debug("Quit event signaled!");
				return;

			case WAIT_TIMEOUT:
				break;

			default:
				log::error("Unhandled WaitForMultipleObjects result !");
				return;
			}
		}
	}

	for (;;)
	{
		if (configFileMonitor.changed())
		{
			configFileMonitor.reset();

			log::info(
				"File change detected ! Reading configuration "
				"file again");

			const auto newConfig =
				config::configuration::from_file(CONSOLE_VARS, configPath);

			for (size_t i = 0; i < console_var_tasks.size(); ++i)
			{
				console_var_tasks[i].cfg.opt_value =
					newConfig.vars[i].opt_value;
			}
		}

		// Apply all variables with a configured value that have changed in
		// memory
		for (const auto& task : console_var_tasks)
		{
			if (task.cfg.opt_value && *task.cvar != task.cfg.opt_value.value())
			{
				*task.cvar = task.cfg.opt_value.value();
			}
		}

		switch (win::wait_on_objects(
			WAIT_TIME_AFTER_VAR_CHECK,
			thread_.quit_event(),
			configFileMonitor.event_handle()))
		{
		case WAIT_OBJECT_0:
			log::info("Quit event signaled!");
			return;

		case WAIT_OBJECT_0 + 1:
			configFileMonitor.on_event_signaled();
			break;

		case WAIT_TIMEOUT:
			break;

		default:
			log::error("Unhandled WaitForMultipleObjects result !");
			return;
		}
	}
}

} // namespace shugoconsole
