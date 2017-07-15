#ifndef TS_TS_H_
#define TS_TS_H_

// README (ugly version):
// Kernel thread is the thread which calls ts::launch_task_system.
// Kernel function is the function passed to ts::launch_task_system. These function is always
// performed inside the kernel thread.
// Each worker thread has a controller fiber. The controller fiber dispathes tasks (inside other fibers)
// ...
// The kernel thread has the controller fiber and an addition fiber (kernel fiber) which is used to run the kernel func.
// The kernel therad never puts the kernel fiber into the fiber pool or fiber wait list.

#include <cassert>
#include <atomic>
#include <functional>
#include <type_traits>
#include <utility>


namespace ts {

using kernel_func_t = void(*)();

struct task_desc final {
	task_desc() noexcept = default;

	template<typename F, typename... Args>
	explicit task_desc(F&& f, Args&&... args)
		: func(std::bind(std::forward<F>(f), std::forward<Args>(args)...))
	{}

	std::function<void()> func;
};

struct task_system_desc final {
	size_t thread_count = 0;
	size_t fiber_count = 0;
	size_t fiber_stack_byte_count = 0;
	size_t queue_size = 0;
	size_t queue_immediate_size = 0;
};

struct task_system_report final {
	// The number of processed tasks with high priority.
	size_t task_immediate_count = 0;

	// The number of processed tasks.
	size_t task_count = 0;
};


inline bool is_valid_task_system_desc(const task_system_desc& desc) noexcept
{
	return (desc.thread_count > 0)
		&& (desc.fiber_count > 0)
		&& (desc.queue_size > 0)
		&& (desc.queue_immediate_size > 0);
}

task_system_report launch_task_system(const task_system_desc& desc, kernel_func_t p_kernel_func);

// How many worker threads are used by the current task system.
// The value is equal to task_system_desc.thread_count
size_t thread_count() noexcept;

void wait_for(const std::atomic_size_t& wait_counter);

void run(std::atomic_size_t* p_wait_counter, task_desc* p_tasks, size_t count);

template<typename F, typename... Args>
inline void run(std::atomic_size_t& wait_counter, F&& f, Args&&... args)
{
	task_desc td(std::forward<F>(f), std::forward<Args>(args)...);
	run(&wait_counter, &td, size_t(1));
}

template<typename F, typename... Args>
inline void run(F&& f, Args&&... args)
{
	task_desc td(std::forward<F>(f), std::forward<Args>(args)...);
	run(nullptr, &td, size_t(1));
}

template<size_t count>
inline void run(std::atomic_size_t& wait_counter, task_desc(&tasks)[count])
{
	run(&wait_counter, tasks, count);
}

template<size_t count>
inline void run(task_desc(&tasks)[count])
{
	run(nullptr, tasks, count);
}

} // namespace ts

#endif // TS_TS_H_
