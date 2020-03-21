#ifndef CRYENGINE_CVAR_HPP
#define CRYENGINE_CVAR_HPP

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <variant>

#include <fmt/format.h>

namespace shugoconsole::cryengine
{
// Aion's CryEngine CVar memory model
// Dummy member types are probably not accurate but they model the gap
// between name and int_value by abusing memory alignment
struct cvar final
{
	cvar() = delete;
	cvar(const cvar&) = delete;
	cvar(cvar&&) = delete;
	cvar& operator=(const cvar&) = delete;
	cvar& operator=(cvar&&) = delete;
	~cvar() = delete;

	using value = std::variant<int, float, std::string>;

	//                                     X86 offset | X86 align | X64 offset | X64 align
	void* dummy0;                       // 0          | 4         | 0          | 8
	char cat;                           // 4          | 1         | 8          | 1
	std::array<char, 128> name;         // 5          | 1         | 9          | 1
	void* dummy1;                       // 133 >> 136 | 4         | 137 >> 144 | 8
	int dummy2;                         // 140        | 4         | 152        | 4
	int dummy3;                         // 144        | 4         | 156        | 4
	int dummy4;                         // 148        | 4         | 160        | 4
	void* dummy5;                       // 152        | 4         | 164 >> 168 | 8
	void* dummy6;                       // 156        | 4         | 176        | 8
	int int_value;                      // 160        | 4         | 184        | 4
	float float_value;                  // 164        | 4         | 188        | 4
	std::array<char, 256> string_value; // 168        | 1         | 192        | 1

	inline void operator=(const cvar::value& v)
	{
		struct assign
		{
			cvar& cvar_;

			void operator()(int i)
			{
				cvar_.int_value = i;
				cvar_.float_value = float(i);
				const auto str = fmt::format("{}", i);
				strncpy_s(
					cvar_.string_value.data(), cvar_.string_value.size(), str.c_str(), str.size());
			}

			void operator()(float f)
			{
				cvar_.int_value = int(f);
				cvar_.float_value = f;
				const auto str = fmt::format("{}", f);
				strncpy_s(
					cvar_.string_value.data(), cvar_.string_value.size(), str.c_str(), str.size());
			}

			void operator()(const std::string& s)
			{
				try
				{
					cvar_.int_value = std::stoi(s);
				}
				catch (std::invalid_argument&)
				{
					cvar_.int_value = 0;
				}
				catch (std::out_of_range&)
				{
					cvar_.int_value = 0;
				}

				try
				{
					cvar_.float_value = std::stof(s);
				}
				catch (std::invalid_argument&)
				{
					cvar_.float_value = 0.0f;
				}
				catch (std::out_of_range&)
				{
					cvar_.float_value = 0.0f;
				}

				strncpy_s(
					cvar_.string_value.data(), cvar_.string_value.size(), s.c_str(), s.size());
			}
		};

		std::visit(assign{*this}, v);
	}
};

// Assert known offsets
#if defined _M_X64
static_assert(offsetof(cvar, cat) == 8);
static_assert(offsetof(cvar, name) == 9);
static_assert(offsetof(cvar, int_value) == 184);
static_assert(offsetof(cvar, float_value) == 188);
static_assert(offsetof(cvar, string_value) == 192);
#elif defined _M_IX86
static_assert(offsetof(cvar, cat) == 4);
static_assert(offsetof(cvar, name) == 5);
static_assert(offsetof(cvar, int_value) == 160);
static_assert(offsetof(cvar, float_value) == 164);
static_assert(offsetof(cvar, string_value) == 168);
#else
#	error "Unrecognized architecture"
#endif

inline static std::string to_string(const cvar::value& v)
{
	struct to_string
	{
		std::string operator()(int i) { return fmt::format("{}", i); }
		std::string operator()(float f) { return fmt::format("{}", f); }
		std::string operator()(const std::string& s) { return s; }
	};

	return std::visit(to_string{}, v);
}

inline cvar::value to_value(const cvar& var, const cvar::value& referenceValue)
{
	struct to_value
	{
		const cvar& cvar_;
		cvar::value operator()(int) { return cvar_.int_value; }
		cvar::value operator()(float) { return cvar_.float_value; }
		cvar::value operator()(const std::string&)
		{
			return std::string{cvar_.string_value.data()};
		}
	};

	return std::visit(to_value{var}, referenceValue);
}

inline std::string to_string(const cvar& var, const cvar::value& referenceValue)
{
	return to_string(to_value(var, referenceValue));
}

inline bool operator==(const cvar& var, const cvar::value & v)
{
	struct equality
	{
		const cvar& cvar_;
		bool operator()(int i) { return cvar_.int_value == i; }
		bool operator()(float f) { return cvar_.float_value == f; }
		bool operator()(const std::string& s)
		{
			return std::strncmp(cvar_.string_value.data(), s.c_str(), s.size() + 1) == 0;
		}
	};

	return std::visit(equality{var}, v);
}

inline bool operator!=(const cvar& var, const cvar::value& v)
{
	return !(var == v);
}

class cvar_pattern final
{
	std::string name_;

  public:
	cvar_pattern(std::string name) noexcept : name_{std::move(name)} {}

	inline std::string name() const { return name_; }

	inline bool match(const cvar* cvar) const
	{
		// Is a valid cvar if type is 0 or 1 and string matches,
		// including trailing null character
		return (cvar->cat == 0 || cvar->cat == 1) &&
			   (std::memcmp(name_.c_str(), cvar->name.data(), name_.size() + 1) == 0);
	}

	// Minimum amount of bytes needed in a buffer to match a cvar_pattern
	inline size_t size() const noexcept { return offsetof(cvar, name) + name_.size() + 1; }
};
} // namespace shugoconsole::cryengine

#endif // CRYENGINE_CVAR_HPP
