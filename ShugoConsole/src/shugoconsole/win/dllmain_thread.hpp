#ifndef SHUGOCONSOLE_WIN_DLLMAIN_THREAD_HPP
#define SHUGOCONSOLE_WIN_DLLMAIN_THREAD_HPP

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace shugoconsole::win
{

// Thread class that *can* run off Windows DllMain (unlike std::thread)
// and behaves properly on process exit
class dllmain_thread
{
public:
	// Mimic std::thread constructors and assignement operators

	dllmain_thread() = default;
	~dllmain_thread();

	template<typename Function, typename ...Args>
	explicit dllmain_thread(Function&& f, Args&&... args) :
		func_(new callable(
			std::forward<Function>(f),
			std::forward<Args>(args)...))
	{
		static_assert(std::is_invocable<Function, Args...>::value);
		start();
	}

	dllmain_thread(dllmain_thread&&) noexcept;
	dllmain_thread& operator=(dllmain_thread&&) noexcept;

	dllmain_thread(const dllmain_thread&) = delete;
	dllmain_thread& operator=(const dllmain_thread&) = delete;

	inline HANDLE quit_event() const { return quit_event_; }

private:
	struct abstract_callable
	{
		virtual ~abstract_callable() = default;
		virtual void operator()() = 0;
	};

	template<typename Function, typename ...Args>
	struct callable : public abstract_callable
	{
		explicit callable(Function&& f, Args&&... args) :
			f_{std::forward<Function>(f)},
			args_{std::forward<Args>(args)...}
		{
		}

		void operator()() override { std::apply(f_, args_); }

		Function f_;
		std::tuple<Args...> args_;
	};

	void start();
	[[noreturn]] void run();
	static DWORD CALLBACK entry_point(void* this_ptr);

	std::unique_ptr<abstract_callable> func_;
	HMODULE module_ = nullptr;
	HANDLE quit_event_ = nullptr;
	HANDLE thread_ = nullptr;
};

} // namespace shugoconsole::win

#endif // SHUGOCONSOLE_WIN_DLLMAIN_THREAD_HPP
