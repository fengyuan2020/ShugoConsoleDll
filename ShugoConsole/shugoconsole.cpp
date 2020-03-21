// Standard library includes
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

// Third-party libraries
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <toml.hpp>

// Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "cryengine_cvar.hpp"
#include "logger.hpp"
#include "memory_parser.hpp"
#include "shugoconsole.hpp"
#include "win_utils.hpp"

namespace shugoconsole
{
using namespace log;
using namespace cryengine;

base_instance::~base_instance() = default;

struct config_cvar final
{
	config_cvar(std::string name, cvar::value defaultValue) :
		name{std::move(name)}, default_value{std::move(defaultValue)}
	{
	}

	std::string name;
	cvar::value default_value;
	cvar* cvar = nullptr;
	std::optional<cvar::value> config;
};

class instance final : public base_instance
{
	// Valid variables and their default values
	std::vector<config_cvar> config_cvar_vec = {{"g_minFov", 90.0f},
												{"g_chatlog", 0},
												{"g_camMax", 28.0f},
												{"d3d9_TripleBuffering", 1},
												{"g_maxfps", 0},
												{"r_Texture_Anisotropic_Level", 8}};

	void run()
	{
		const auto modulePath = WinUtils::GetModulePath(module_);
		info("Executable path: {}", modulePath.u8string());

		const auto configPath = modulePath.parent_path() / "shugoconsole.toml";
		info("Config file path: {}", configPath.u8string());

		read_config_file(configPath);

		// Find all configurable CVars in memory
		{
			std::vector<std::byte> buffer(64 * 1024);
			for (;;)
			{
				// Assuming that config_cvar_vec is not empty and that we test all config variables
				// after scanning memory, there is at least one matching element in the vector
				config_cvar& ccvar = *std::find_if(
					config_cvar_vec.begin(),
					config_cvar_vec.end(),
					[](const config_cvar& ccvar) -> bool { return ccvar.cvar == nullptr; });

				ccvar.cvar = find_cvar_ptr({ccvar.name}, buffer);

				if (ccvar.cvar)
				{
					info(
						"Found {} current={}",
						ccvar.name,
						to_string(*ccvar.cvar, ccvar.default_value));
				}

				if (std::all_of(
						config_cvar_vec.begin(),
						config_cvar_vec.end(),
						[](const config_cvar& ccvar) -> bool { return ccvar.cvar != nullptr; }))
				{
					info("Found all configurable CVars !");
					break;
				}

				// Test if we have to quit the thread
				// In case we have not found the variable, wait 1 second before continuing
				if (::WaitForSingleObject(quit_event_, ccvar.cvar != nullptr ? 0 : 1000) !=
					WAIT_TIMEOUT)
				{
					debug("Quit event signaled!");
					return;
				}
			}
		}

		for (;;)
		{
			// Apply all variables that have changed in memory
			for (const auto& ccvar : config_cvar_vec)
				if (ccvar.config && *ccvar.cvar != ccvar.config.value())
					*ccvar.cvar = ccvar.config.value();

			// wait 1 second or quit event
			if (::WaitForSingleObject(quit_event_, 1000) != WAIT_TIMEOUT)
			{
				info("Quit event signaled!");
				return;
			}
		}
	}

	void read_config_file(std::filesystem::path configPath)
	{
		// Use ifstream to open file because toml11 does not
		// support wchar_t filenames
		std::ifstream configStream{configPath.wstring()};
		if (!configStream.is_open())
			warn("Could not open configuration file!");

		toml::value root;
		try
		{
			root = toml::parse(configStream);
		}
		catch (toml::exception& e)
		{
			error("TOML parsing error: {}", e.what());

			for (auto& ccvar : config_cvar_vec)
				ccvar.config = std::nullopt;
			return;
		}

		for (auto& ccvar : config_cvar_vec)
		{
			toml::value val;
			try
			{
				val = toml::find(root, ccvar.name);
			}
			catch (std::out_of_range&)
			{
				// key not found
				ccvar.config = std::nullopt;
				continue;
			}
			catch (toml::exception& e)
			{
				error("TOML error while reading value of {}: {}", ccvar.name, e.what());

				ccvar.config = std::nullopt;
				continue;
			}

			struct apply_config_cvar
			{
				config_cvar& ccvar;
				const toml::value& v;

				void operator()(int)
				{
					if (v.is_integer())
						ccvar.config = static_cast<int>(v.as_integer());
					else if (v.is_boolean())
						ccvar.config = v.as_boolean() ? 1 : 0;
					else
						error(
							"'{}' should have an integer or a boolean value, found '{}' instead!",
							ccvar.name,
							toml::format(v));
				}

				void operator()(float)
				{
					if (v.is_integer())
						ccvar.config = static_cast<float>(v.as_integer());
					else if (v.is_floating())
						ccvar.config = static_cast<float>(v.as_floating());
					else
						error(
							"'{}' should have an integer or a floating point value, found '{}' "
							"instead!"
							"instead!",
							ccvar.name,
							toml::format(v));
				}

				void operator()(const std::string&)
				{
					if (v.is_string())
						ccvar.config = v.as_string();
					else
						error(
							"'{}' should have a string value, found '{}' instead!",
							ccvar.name,
							toml::format(v));
				}
			};

			std::visit(apply_config_cvar{ccvar, val}, ccvar.default_value);

			if (ccvar.config)
				info("{}={}", ccvar.name, to_string(ccvar.config.value()));
		}
	}

	HMODULE module_ = nullptr;
	HANDLE quit_event_ = nullptr;
	HANDLE thread_ = nullptr;

	void Thread()
	{
		debug("Entered ShugoConsole thread!");

		try
		{
			run();
		}
		catch (std::exception& e)
		{
			critical("Critical Exception: {}", e.what());
		}

		debug("Waiting for thread quit event...");

		::WaitForSingleObject(quit_event_, INFINITE);
		::CloseHandle(quit_event_);
		::FreeLibraryAndExitThread(module_, 0);
	}

	static DWORD CALLBACK ThreadEntryPoint(void* thisPtr)
	{
		static_cast<instance*>(thisPtr)->Thread();
	}

  public:
	instance()
	{
		// Increment current module reference Count
		module_ = WinUtils::GetCurrentModuleHandle();

		// Setup logger
		setup_logger();

		if (config_cvar_vec.size() == 0)
		{
			critical("Check your code, there are no configurable CVars!");
			return;
		}

		// Create Thread
		debug("Creating ShugoConsole thread...");

		// Use windows functions instead of std::thread because
		// this will probably be called from DllMain
		// so we must be very careful
		quit_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
		DWORD dwThreadId = 0;
		thread_ = ::CreateThread(nullptr, 0, &ThreadEntryPoint, nullptr, 0, &dwThreadId);
	}

	~instance()
	{
		// When called from DllMain's DLL_PROCESS_DETACH,
		// In case of TerminateProcess, try exitting the thread gracefully
		// In case of ExitProcess, the thread will be stopped forcefully
		// and g_Thread will be in the signalled state
		::SetEvent(quit_event_);
		::WaitForSingleObject(thread_, INFINITE);
	}
};

[[nodiscard]] std::unique_ptr<base_instance> create()
{
	return std::make_unique<instance>();
}

} // namespace shugoconsole
