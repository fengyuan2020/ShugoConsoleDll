#ifndef SHUGOCONSOLE_CRY_CVAR_HPP
#define SHUGOCONSOLE_CRY_CVAR_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>

namespace shugoconsole::cry
{

// Aion's CryEngine CVar memory model
// Dummy member types are probably not accurate but they model the gap
// between name and int_value by abusing memory alignment
struct cvar final
{
	// o = offset, a = align               o32 | a32 | o64 | a64
	void* dummy0;                       // 0   | 4   | 0   | 8
	char cat;                           // 4   | 1   | 8   | 1
	std::array<char, 128> name;         // 5   | 1   | 9   | 1
	void* dummy1;                       // 136 | 4   | 144 | 8
	int dummy2;                         // 140 | 4   | 152 | 4
	int dummy3;                         // 144 | 4   | 156 | 4
	int dummy4;                         // 148 | 4   | 160 | 4
	void* dummy5;                       // 152 | 4   | 168 | 8
	void* dummy6;                       // 156 | 4   | 176 | 8
	int int_value;                      // 160 | 4   | 184 | 4
	float float_value;                  // 164 | 4   | 188 | 4
	std::array<char, 256> string_value; // 168 | 1   | 192 | 1

	// Enum of types a cvar can store
	enum class type
	{
		integer,
		floating,
		string
	};

	// Variant corresponding to the types a cvar can store
	using value = std::variant<int, float, std::string>;

	// Pattern to recognize a cvar in memory
	class pattern final
	{
		std::string name_;

	public:
		pattern(std::string name) noexcept : name_{std::move(name)} {}

		inline std::string name() const { return name_; }

		// true if a v is a valid cvar if type is 0 or 1 and string matches,
		// including trailing null character
		inline bool match(const cvar* var) const
		{
			return (var->cat == 0 || var->cat == 1) &&
				   (std::memcmp(
						name_.c_str(),
						var->name.data(),
						name_.size() + 1) == 0);
		}

		// Minimum amount of bytes needed in a buffer to match a cvar_pattern
		inline size_t size() const noexcept
		{
			return offsetof(cvar, name) + name_.size() + 1;
		}
	};

	// Can't be constructed, detroyed, copied or moved
	// The only way to make a cvar is to get a pointer to an
	// existing instance in memory
	cvar() = delete;
	cvar(const cvar&) = delete;
	cvar(cvar&&) = delete;
	cvar& operator=(const cvar&) = delete;
	cvar& operator=(cvar&&) = delete;
	~cvar() = delete;

	// Set all fields
	inline void set(int i, float f, const std::string& s)
	{
		int_value = i;
		float_value = f;
		::strncpy_s(
			string_value.data(),
			string_value.size(),
			s.c_str(),
			s.size());
	}

	// Set int field and cast value for float and string fields
	inline cvar& operator=(int i)
	{
		set(i, float(i), std::to_string(i));
		return *this;
	}

	// Set float field and cast value for int and string fields
	inline cvar& operator=(float f)
	{
		set(int(f), f, std::to_string(f));
		return *this;
	}

	// Set string field and cast value for int and float fields
	inline cvar& operator=(const std::string& s)
	{
		int i = 0;
		std::from_chars(s.data(), s.data() + s.size(), i);
		float f = 0.0f;
		std::from_chars(s.data(), s.data() + s.size(), f);
		set(i, f, s);
		return *this;
	}

	// Set fields depending on the type of the value
	inline cvar& operator=(const value& v)
	{
		return std::visit([this](auto&& x) -> cvar& { return *this = x; }, v);
	}

	// Convert cvar to value variant
	inline value to_value(type t) const
	{
		switch (t)
		{
		case type::integer:
			return int_value;
		case type::floating:
			return float_value;
		case type::string:
			return std::string{string_value.data()};
		default:
			throw std::out_of_range(
				"shugoconsole::cry::cvar::to_value: invalid cvar_type");
		}
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

inline std::string to_string(const cvar::value& val)
{
	struct visit_to_string
	{
		std::string operator()(int i) { return std::to_string(i); }
		std::string operator()(float f) { return std::to_string(f); }
		std::string operator()(const std::string& s) { return s; }
	};

	return std::visit(visit_to_string{}, val);
}

inline bool operator==(const cvar& v, int i)
{
	return v.int_value == i;
}

inline bool operator==(const cvar& v, float f)
{
	return v.float_value == f;
}

inline bool operator==(const cvar& v, const std::string& s)
{
	return std::strncmp(
			   v.string_value.data(),
			   s.c_str(),
			   std::min(v.string_value.size(), s.size() + 1)) == 0;
}

inline bool operator==(const cvar& v, cvar::value val)
{
	return std::visit([&v](auto&& x) { return v == x; }, val);
}

template<typename T>
inline bool operator!=(const cvar& var, T&& v)
{
	return !(var == std::forward<T>(v));
}

} // namespace shugoconsole::cry

#endif // SHUGOCONSOLE_CRY_CVAR_HPP
