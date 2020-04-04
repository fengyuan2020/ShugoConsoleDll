#ifndef SHUGOCONSOLE_HPP
#define SHUGOCONSOLE_HPP

#include <memory>

namespace shugoconsole
{

// The only thing you can do with a ShugoConsole interface is destroy it
class instance
{
public:
	virtual ~instance();
};

[[nodiscard]] std::unique_ptr<instance> create();

} // namespace shugoconsole

#endif // SHUGOCONSOLE_HPP
