#include "shugoconsole.hpp"

#include <thread>
int main()
{
	std::this_thread::sleep_for(std::chrono::seconds(5));
	Dummy();
	return 0;
}