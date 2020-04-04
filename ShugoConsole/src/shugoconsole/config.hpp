#ifndef SHUGOCONSOLE_CONFIG_HPP
#define SHUGOCONSOLE_CONFIG_HPP

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <outcome.hpp>
#include <toml.hpp>

#include "shugoconsole/cry/cvar.hpp"
#include "shugoconsole/log.hpp"

namespace shugoconsole::config
{

struct error final
{
	error() = default;

	template<typename T, typename... Args>
	explicit error(T&& format, Args&&... args) :
		message{fmt::format(std::forward<T>(format), std::forward<Args>(args)...)}
	{
	}

	std::string message;
};

using result = OUTCOME_V2_NAMESPACE::checked<cry::cvar::value, error>;

namespace types
{

namespace detail
{

template<typename T>
struct minmax final
{
	static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);

	T min, max;
};

template<typename T>
static result check_bounds(minmax<T> m, T v)
{
	if (v < m.min || v > m.max)
	{
		return error(
			"{} is not a valid value. Value must be between {} and {}.",
			m.min,
			m.max,
			v);
	}

	return v;
}

} // namespace detail

// type concept
// ------------
// ```
// cry::cvar::type cvar_type() const
// result from_toml(const toml::value& toml_value) const
// ```

class boolean final
{
public:
	boolean() = default;

	cry::cvar::type cvar_type() const { return cry::cvar::type::integer; }

	result from_toml(const toml::value& toml_value) const
	{
		if (toml_value.is_integer())
		{
			return check(static_cast<int>(toml_value.as_integer()));
		}
		else if (toml_value.is_boolean())
		{
			return check(toml_value.as_boolean());
		}
		else
		{
			return error(
				"'{}' is not a valid value. It should "
				"be a boolean or an integer value (0 or "
				"1).",
				toml::format(toml_value));
		}
	}

private:
	result check(int i) const
	{
		if (i != 0 && i != 1)
		{
			return error(
				"{} is not a valid value. Boolean values must be "
				"either 0, 1, true or false.",
				i);
		}

		return i;
	}

	result check(bool b) const { return b ? 1 : 0; }
};

struct integer final
{
public:
	integer() = default;

	static integer with_min_max(int min, int max)
	{
		return {constraint_minmax{detail::minmax<int>{min, max}}};
	};

	static integer with_values(std::vector<int> v)
	{
		return {constraint_values{std::move(v)}};
	}

	cry::cvar::type cvar_type() const { return cry::cvar::type::integer; }

	result from_toml(const toml::value& toml_value) const
	{
		if (toml_value.is_integer())
		{
			return check(static_cast<int>(toml_value.as_integer()));
		}
		else
		{
			return config::error(
				"'{}' is not a valid value. It should "
				"be an integer.",
				toml::format(toml_value));
		}
	}

private:
	template<typename T>
	integer(T&& c) : constraint(std::forward<T>(c))
	{
	}

	result check(int i) const
	{
		return std::visit(
			[i](const auto& c) -> result { return c.check(i); },
			constraint);
	}

	struct no_constraint
	{
		result check(int i) const { return i; }
	};

	struct constraint_minmax
	{
		detail::minmax<int> m;
		result check(int i) const { return detail::check_bounds(m, i); }
	};

	struct constraint_values
	{
		std::vector<int> v;
		result check(int i) const
		{
			if (v.empty() || std::any_of(v.begin(), v.end(), [i](int value) {
					return i == value;
				}))
			{
				return i;
			}
			else
			{
				return error(
					"{} is not a valid value. It should be one of: {}.",
					i,
					fmt::join(v, ", "));
			}
		}
	};

	std::variant<no_constraint, constraint_minmax, constraint_values>
		constraint = no_constraint{};
};

class floating final
{
public:
	floating() = default;

	static floating with_min_max(float min, float max)
	{
		return {constraint_minmax{detail::minmax<float>{min, max}}};
	};

	cry::cvar::type cvar_type() const { return cry::cvar::type::floating; }

	result from_toml(const toml::value& toml_value) const
	{
		if (toml_value.is_integer())
		{
			return check(static_cast<int>(toml_value.as_integer()));
		}
		else if (toml_value.is_floating())
		{
			return check(static_cast<float>(toml_value.as_floating()));
		}
		else
		{
			return config::error(
				"'{}' is not a valid value. It should "
				"be an integer or a floating point "
				"value.",
				toml::format(toml_value));
		}
	}

private:
	template<typename T>
	floating(T&& c) : constraint(std::forward<T>(c))
	{
	}

	result check(int i) const { return check(static_cast<float>(i)); }
	result check(float f) const
	{
		return std::visit(
			[f](const auto& c) -> result { return c.check(f); },
			constraint);
	}

	struct no_constraint
	{
		result check(float f) const { return f; }
	};

	struct constraint_minmax
	{
		detail::minmax<float> m;
		result check(float f) const { return detail::check_bounds(m, f); }
	};

	std::variant<no_constraint, constraint_minmax> constraint = no_constraint{};
};

class string final
{
public:
	string() = default;

	static string with_values(std::vector<std::string> v)
	{
		return {constraint_values{std::move(v)}};
	}

	cry::cvar::type cvar_type() const { return cry::cvar::type::string; }

	result from_toml(const toml::value& toml_value) const
	{
		if (toml_value.is_string())
		{
			return check(toml_value.as_string());
		}
		else
		{
			return config::error(
				"'{}' is not a valid value. It should "
				"be a string value.",
				toml::format(toml_value));
		}
	}

private:
	template<typename T>
	string(T&& c) : constraint(std::forward<T>(c))
	{
	}

	result check(const std::string& s) const
	{
		constexpr size_t max_size =
			std::tuple_size<decltype(std::declval<cry::cvar>().string_value)>();

		if (s.size() >= max_size)
		{
			return error(
				"'{}' is not a valid value. String values must "
				"contain 255 characters or less.",
				s);
		}

		return std::visit(
			[&s](const auto& c) -> result { return c.check(s); },
			constraint);
	}

	struct no_constraint
	{
		result check(const std::string& s) const { return s; }
	};

	struct constraint_values
	{
		std::vector<std::string> v;
		result check(const std::string& s) const
		{
			if (v.empty() ||
				std::any_of(v.begin(), v.end(), [&s](const std::string& value) {
					return s == value;
				}))
			{
				return s;
			}
			else
			{
				return error(
					"{} is not a valid value. It should be one of: {}.",
					s,
					fmt::join(v, ", "));
			}
		}
	};

	std::variant<no_constraint, constraint_values> constraint = no_constraint{};
};

} // namespace types

class configuration final
{
public:
	struct variable_definition final
	{
		using type_variant = std::variant<
			types::boolean,
			types::integer,
			types::floating,
			types::string>;

		cry::cvar::type cvar_type() const
		{
			return std::visit(
				[](auto&& t) -> cry::cvar::type { return t.cvar_type(); },
				type);
		}

		std::string name;
		type_variant type;
	};

	using variable_definition_set = std::initializer_list<variable_definition>;

	struct variable
	{
		const variable_definition def;
		std::optional<cry::cvar::value> opt_value;
	};

	std::vector<variable> vars;

	// Creates a configuration with empty values
	explicit configuration(variable_definition_set var_set)
	{
		std::transform(
			var_set.begin(),
			var_set.end(),
			std::back_inserter(vars),
			[](const variable_definition& def) {
				return variable{def, std::nullopt};
			});
	}

	static configuration from_file(
		const variable_definition_set& varSet,
		std::filesystem::path configPath)
	{
		// Use ifstream to open file because toml11 does not
		// support wchar_t filenames
		std::ifstream configStream{configPath.wstring()};

		if (!configStream.is_open())
		{
			log::warn(
				"Could not open configuration file '{}' for reading.",
				configPath.u8string());
			return configuration{varSet};
		}

		try
		{
			const auto root = toml::parse(configStream);

			configuration cfg{varSet};
			for (variable& var : cfg.vars)
			{
				try
				{
					const auto toml_value = toml::find(root, var.def.name);
					const auto result = std::visit(
						[&](const auto& t) { return t.from_toml(toml_value); },
						var.def.type);

					if (result)
					{
						var.opt_value = result.value();

						log::info(
							"{}={}",
							var.def.name,
							cry::to_string(var.opt_value.value()));
					}
					else
					{
						var.opt_value = std::nullopt;

						log::error(
							"'{}': {}",
							var.def.name,
							result.error().message);
					}
				}
				catch (std::out_of_range&) // key not found
				{
					var.opt_value = std::nullopt;
				}
				catch (toml::exception& e)
				{
					var.opt_value = std::nullopt;
					log::error(
						"'{}': error while reading value: {}",
						var.def.name,
						e.what());
				}
			}

			return cfg;
		}
		catch (toml::exception& e)
		{
			log::error(
				"Error while parsing configuration file '{}': {}",
				configPath.u8string(),
				e.what());

			return configuration{varSet};
		}
	}
};

} // namespace shugoconsole::config

#endif // !SHUGOCONSOLE_CONFIG_HPP
